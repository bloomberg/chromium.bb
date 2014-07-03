// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <pk11pub.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/tpm_token_loader.h"
#include "components/onc/onc_constants.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kWifiStub = "wifi_stub";
const char* kWifiSSID = "wifi_ssid";
const char* kUserProfilePath = "user_profile";
const char* kUserHash = "user_hash";

}  // namespace

class ClientCertResolverTest : public testing::Test {
 public:
  ClientCertResolverTest() : service_test_(NULL),
                             profile_test_(NULL),
                             cert_loader_(NULL),
                             user_(kUserHash) {
  }
  virtual ~ClientCertResolverTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize NSS db for the user.
    ASSERT_TRUE(user_.constructed_successfully());
    user_.FinishInit();
    private_slot_ = crypto::GetPrivateSlotForChromeOSUser(
        user_.username_hash(),
        base::Callback<void(crypto::ScopedPK11Slot)>());
    ASSERT_TRUE(private_slot_.get());
    test_nssdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    test_nssdb_->SetSlowTaskRunnerForTest(message_loop_.message_loop_proxy());

    DBusThreadManager::InitializeWithStub();
    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    profile_test_ =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    base::RunLoop().RunUntilIdle();
    service_test_->ClearServices();
    base::RunLoop().RunUntilIdle();

    TPMTokenLoader::InitializeForTest();

    CertLoader::Initialize();
    cert_loader_ = CertLoader::Get();
    cert_loader_->force_hardware_backed_for_test();
  }

  virtual void TearDown() OVERRIDE {
    client_cert_resolver_.reset();
    managed_config_handler_.reset();
    network_config_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    DBusThreadManager::Shutdown();
    CleanupSlotContents();
  }

 protected:
  void StartCertLoader() {
    cert_loader_->StartWithNSSDB(test_nssdb_.get());
    if (test_client_cert_) {
      test_pkcs11_id_ = base::StringPrintf(
          "%i:%s",
          cert_loader_->TPMTokenSlotID(),
          CertLoader::GetPkcs11IdForCert(*test_client_cert_).c_str());
      ASSERT_TRUE(!test_pkcs11_id_.empty());
    }
  }

  // Imports a CA cert (stored as PEM in test_ca_cert_pem_) and a client
  // certificate signed by that CA. Its PKCS#11 ID is stored in
  // |test_pkcs11_id_|.
  void SetupTestCerts() {
    // Import a CA cert.
    net::CertificateList ca_cert_list =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "websocket_cacert.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(!ca_cert_list.empty());
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nssdb_->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    ASSERT_TRUE(failures.empty()) << net::ErrorToString(failures[0].net_error);

    net::X509Certificate::GetPEMEncoded(ca_cert_list[0]->os_cert_handle(),
                                        &test_ca_cert_pem_);
    ASSERT_TRUE(!test_ca_cert_pem_.empty());

    // Import a client cert signed by that CA.
    std::string pkcs12_data;
    ASSERT_TRUE(base::ReadFileToString(
        net::GetTestCertsDirectory().Append("websocket_client_cert.p12"),
        &pkcs12_data));

    net::CertificateList client_cert_list;
    scoped_refptr<net::CryptoModule> module(
        net::CryptoModule::CreateFromHandle(private_slot_.get()));
    ASSERT_EQ(
        net::OK,
        test_nssdb_->ImportFromPKCS12(
            module, pkcs12_data, base::string16(), false, &client_cert_list));
    ASSERT_TRUE(!client_cert_list.empty());
    test_client_cert_ = client_cert_list[0];
  }

  void SetupNetworkHandlers() {
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_profile_handler_.reset(new NetworkProfileHandler());
    network_config_handler_.reset(new NetworkConfigurationHandler());
    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    client_cert_resolver_.reset(new ClientCertResolver());

    network_profile_handler_->Init();
    network_config_handler_->Init(network_state_handler_.get());
    managed_config_handler_->Init(network_state_handler_.get(),
                                  network_profile_handler_.get(),
                                  network_config_handler_.get(),
                                  NULL /* network_device_handler */);
    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_config_handler_.get());
    client_cert_resolver_->SetSlowTaskRunnerForTest(
        message_loop_.message_loop_proxy());

    profile_test_->AddProfile(kUserProfilePath, kUserHash);
  }

  void SetupWifi() {
    service_test_->SetServiceProperties(kWifiStub,
                                        kWifiStub,
                                        kWifiSSID,
                                        shill::kTypeWifi,
                                        shill::kStateOnline,
                                        true /* visible */);
    // Set an arbitrary cert id, so that we can check afterwards whether we
    // cleared the property or not.
    service_test_->SetServiceProperty(
        kWifiStub, shill::kEapCertIdProperty, base::StringValue("invalid id"));
    profile_test_->AddService(kUserProfilePath, kWifiStub);

    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->AddManagerService(kWifiStub, true);
  }

  // Setup a policy with a certificate pattern that matches any client cert that
  // is signed by the test CA cert (stored in |test_ca_cert_pem_|). In
  // particular it will match the test client cert.
  void SetupPolicy() {
    const char* kTestPolicyTemplate =
        "[ { \"GUID\": \"wifi_stub\","
        "    \"Name\": \"wifi_stub\","
        "    \"Type\": \"WiFi\","
        "    \"WiFi\": {"
        "      \"Security\": \"WPA-EAP\","
        "      \"SSID\": \"wifi_ssid\","
        "      \"EAP\": {"
        "        \"Outer\": \"EAP-TLS\","
        "        \"ClientCertType\": \"Pattern\","
        "        \"ClientCertPattern\": {"
        "          \"IssuerCAPEMs\": [ \"%s\" ]"
        "        }"
        "      }"
        "    }"
        "} ]";
    std::string policy_json =
        base::StringPrintf(kTestPolicyTemplate, test_ca_cert_pem_.c_str());

    std::string error;
    scoped_ptr<base::Value> policy_value(base::JSONReader::ReadAndReturnError(
        policy_json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error));
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = NULL;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    managed_config_handler_->SetPolicy(
        onc::ONC_SOURCE_USER_POLICY,
        kUserHash,
        *policy,
        base::DictionaryValue() /* no global network config */);
  }

  void GetClientCertProperties(std::string* pkcs11_id) {
    pkcs11_id->clear();
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kWifiStub);
    if (!properties)
      return;
    properties->GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                              pkcs11_id);
  }

  ShillServiceClient::TestInterface* service_test_;
  ShillProfileClient::TestInterface* profile_test_;
  std::string test_pkcs11_id_;
  scoped_refptr<net::X509Certificate> test_ca_cert_;
  std::string test_ca_cert_pem_;
  base::MessageLoop message_loop_;

 private:
  void CleanupSlotContents() {
    CERTCertList* cert_list = PK11_ListCertsInSlot(private_slot_.get());
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      scoped_refptr<net::X509Certificate> cert(
          net::X509Certificate::CreateFromHandle(
              node->cert, net::X509Certificate::OSCertHandles()));
      test_nssdb_->DeleteCertAndKey(cert.get());
    }
    CERT_DestroyCertList(cert_list);
  }

  CertLoader* cert_loader_;
  scoped_refptr<net::X509Certificate> test_client_cert_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_config_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl> managed_config_handler_;
  scoped_ptr<ClientCertResolver> client_cert_resolver_;
  crypto::ScopedTestNSSChromeOSUser user_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_nssdb_;
  crypto::ScopedPK11Slot private_slot_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertResolverTest);
};

TEST_F(ClientCertResolverTest, NoMatchingCertificates) {
  SetupNetworkHandlers();
  SetupWifi();
  StartCertLoader();
  SetupPolicy();
  base::RunLoop().RunUntilIdle();

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
}

TEST_F(ClientCertResolverTest, ResolveOnCertificatesLoaded) {
  SetupNetworkHandlers();
  SetupWifi();
  SetupTestCerts();
  SetupPolicy();
  base::RunLoop().RunUntilIdle();

  StartCertLoader();
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_pkcs11_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, ResolveAfterPolicyApplication) {
  SetupTestCerts();
  StartCertLoader();
  SetupNetworkHandlers();
  SetupWifi();
  base::RunLoop().RunUntilIdle();

  // Policy application will trigger the ClientCertResolver.
  SetupPolicy();
  base::RunLoop().RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetClientCertProperties(&pkcs11_id);
  EXPECT_EQ(test_pkcs11_id_, pkcs11_id);
}

}  // namespace chromeos
