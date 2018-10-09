// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <pk11pub.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

constexpr char kWifiStub[] = "wifi_stub";
constexpr char kWifiSSID[] = "wifi_ssid";
constexpr char kUserProfilePath[] = "user_profile";
constexpr char kUserHash[] = "user_hash";

}  // namespace

class ClientCertResolverTest : public testing::Test,
                               public ClientCertResolver::Observer {
 public:
  ClientCertResolverTest() = default;
  ~ClientCertResolverTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());
    ASSERT_TRUE(test_system_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));

    DBusThreadManager::Initialize();
    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    profile_test_ =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    profile_test_->AddProfile(kUserProfilePath, kUserHash);
    scoped_task_environment_.RunUntilIdle();
    service_test_->ClearServices();
    scoped_task_environment_.RunUntilIdle();

    NetworkCertLoader::Initialize();
    network_cert_loader_ = NetworkCertLoader::Get();
    NetworkCertLoader::ForceHardwareBackedForTesting();
  }

  void TearDown() override {
    if (client_cert_resolver_)
      client_cert_resolver_->RemoveObserver(this);
    client_cert_resolver_.reset();
    test_clock_.reset();
    if (network_state_handler_)
      network_state_handler_->Shutdown();
    managed_config_handler_.reset();
    network_config_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    NetworkCertLoader::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  void StartNetworkCertLoader() {
    network_cert_loader_->SetUserNSSDB(test_nsscertdb_.get());
    if (test_client_cert_.get()) {
      int slot_id = 0;
      const std::string pkcs11_id =
          NetworkCertLoader::GetPkcs11IdAndSlotForCert(test_client_cert_.get(),
                                                       &slot_id);
      test_cert_id_ = base::StringPrintf("%i:%s", slot_id, pkcs11_id.c_str());
    }
  }

  // Imports a client certificate. After a subsequent StartNetworkCertLoader()
  // invocation, the PKCS#11 ID of the imported certificate will be stored in
  // |test_cert_id_|.  If |import_issuer| is true, also imports the CA cert
  // (stored as PEM in test_ca_cert_pem_) that issued the client certificate.
  void SetupTestCerts(const std::string& prefix, bool import_issuer) {
    // Load a CA cert.
    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), prefix + "_ca.pem",
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(!ca_cert_list.empty());
    net::x509_util::GetPEMEncoded(ca_cert_list[0].get(), &test_ca_cert_pem_);
    ASSERT_TRUE(!test_ca_cert_pem_.empty());

    if (import_issuer) {
      net::NSSCertDatabase::ImportCertFailureList failures;
      EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
          ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
      ASSERT_TRUE(failures.empty())
          << net::ErrorToString(failures[0].net_error);
    }

    // Import a client cert signed by that CA.
    net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                        prefix + ".pem", prefix + ".pk8",
                                        test_nssdb_.slot(), &test_client_cert_);
    ASSERT_TRUE(test_client_cert_.get());
  }

  // Imports a client certificate with a subject CommonName encoded as
  // PrintableString, but containing invalid characters. It is imported into the
  // user slot. After a subsequent StartNetworkCertLoader() invocation, the
  // PKCS#11 ID of the imported certificate will be stored in |test_cert_id_|.
  void SetupTestCertWithBadPrintableString() {
    base::FilePath certs_dir = net::GetTestNetDataDirectory().AppendASCII(
        "parse_certificate_unittest");
    ASSERT_TRUE(net::ImportSensitiveKeyFromFile(
        certs_dir, "v3_certificate_template.pk8", test_nssdb_.slot()));

    std::string file_data;
    ASSERT_TRUE(base::ReadFileToString(
        certs_dir.AppendASCII(
            "subject_printable_string_containing_utf8_client_cert.pem"),
        &file_data));

    net::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
    ASSERT_TRUE(pem_tokenizer.GetNext());
    std::string cert_der(pem_tokenizer.data());
    ASSERT_FALSE(pem_tokenizer.GetNext());

    test_client_cert_ = net::x509_util::CreateCERTCertificateFromBytes(
        reinterpret_cast<const uint8_t*>(cert_der.data()), cert_der.size());
    ASSERT_TRUE(test_client_cert_);

    ASSERT_TRUE(net::ImportClientCertToSlot(test_client_cert_.get(),
                                            test_nssdb_.slot()));
  }

  void SetupTestCertInSystemToken(const std::string& prefix) {
    test_nsscertdb_->SetSystemSlot(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_system_nssdb_.slot())));

    net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), prefix + ".pem", prefix + ".pk8",
        test_system_nssdb_.slot(), &test_client_cert_);
    ASSERT_TRUE(test_client_cert_.get());
  }

  void SetupNetworkHandlers() {
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_profile_handler_.reset(new NetworkProfileHandler());
    network_config_handler_.reset(new NetworkConfigurationHandler());
    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    client_cert_resolver_.reset(new ClientCertResolver());

    test_clock_.reset(new base::SimpleTestClock);
    test_clock_->SetNow(base::Time::Now());
    client_cert_resolver_->SetClockForTesting(test_clock_.get());

    network_profile_handler_->Init();
    network_config_handler_->Init(network_state_handler_.get(),
                                  nullptr /* network_device_handler */);
    managed_config_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        nullptr /* prohibited_technologies_handler */);
    // Run all notifications before starting the cert loader to reduce run time.
    scoped_task_environment_.RunUntilIdle();

    client_cert_resolver_->Init(network_state_handler_.get(),
                                managed_config_handler_.get());
    client_cert_resolver_->AddObserver(this);
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
    service_test_->SetServiceProperty(kWifiStub, shill::kEapCertIdProperty,
                                      base::Value("invalid id"));
    profile_test_->AddService(kUserProfilePath, kWifiStub);

    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->AddManagerService(kWifiStub, true);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // with a certain Issuer CN. It will match the test client cert imported by
  // SetupTestCerts.
  void SetupPolicyMatchingIssuerCN(onc::ONCSource onc_source) {
    const char* test_policy =
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
        "          \"Issuer\": {"
        "            \"CommonName\": \"B CA\""
        "          }"
        "        }"
        "      }"
        "    }"
        "} ]";

    std::string error;
    std::unique_ptr<base::Value> policy_value =
        base::JSONReader::ReadAndReturnError(
            test_policy, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = nullptr;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    std::string user_hash =
        onc_source == onc::ONC_SOURCE_USER_POLICY ? kUserHash : "";
    managed_config_handler_->SetPolicy(
        onc_source, user_hash, *policy,
        base::DictionaryValue() /* no global network config */);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // with a Subject Organization set to "Blar". It will match the test client
  // cert imported by SetupTestCertWithBadPrintableString.
  void SetupPolicyMatchingSubjectOrgForBadPrintableStringCert(
      onc::ONCSource onc_source) {
    const char* test_policy =
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
        "          \"Subject\": {"
        "            \"Organization\": \"Blar\""
        "          }"
        "        }"
        "      }"
        "    }"
        "} ]";

    std::string error;
    std::unique_ptr<base::Value> policy_value =
        base::JSONReader::ReadAndReturnError(
            test_policy, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = nullptr;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    std::string user_hash =
        onc_source == onc::ONC_SOURCE_USER_POLICY ? kUserHash : "";
    managed_config_handler_->SetPolicy(
        onc_source, user_hash, *policy,
        base::DictionaryValue() /* no global network config */);
  }

  void SetupCertificateConfigMatchingIssuerCN(
      onc::ONCSource onc_source,
      client_cert::ClientCertConfig* client_cert_config) {
    const char* test_onc_pattern =
        "{"
        "  \"Issuer\": {"
        "    \"CommonName\": \"B CA\""
        "  }"
        "}";
    std::string error;
    std::unique_ptr<base::Value> onc_pattern_value =
        base::JSONReader::ReadAndReturnError(test_onc_pattern,
                                             base::JSON_ALLOW_TRAILING_COMMAS,
                                             nullptr, &error);
    ASSERT_TRUE(onc_pattern_value) << error;

    base::DictionaryValue* onc_pattern_dict;
    onc_pattern_value->GetAsDictionary(&onc_pattern_dict);

    client_cert_config->onc_source = onc_source;
    client_cert_config->pattern.ReadFromONCDictionary(*onc_pattern_dict);
  }

  // Sets up a policy with a certificate pattern that matches any client cert
  // that is signed by the test CA cert (stored in |test_ca_cert_pem_|). In
  // particular it will match the test client cert.
  void SetupPolicyMatchingIssuerPEM(onc::ONCSource onc_source,
                                    const std::string& identity) {
    const char* test_policy_template =
        "[ { \"GUID\": \"wifi_stub\","
        "    \"Name\": \"wifi_stub\","
        "    \"Type\": \"WiFi\","
        "    \"WiFi\": {"
        "      \"Security\": \"WPA-EAP\","
        "      \"SSID\": \"wifi_ssid\","
        "      \"EAP\": {"
        "        \"Identity\": \"%s\","
        "        \"Outer\": \"EAP-TLS\","
        "        \"ClientCertType\": \"Pattern\","
        "        \"ClientCertPattern\": {"
        "          \"IssuerCAPEMs\": [ \"%s\" ]"
        "        }"
        "      }"
        "    }"
        "} ]";
    std::string policy_json = base::StringPrintf(
        test_policy_template, identity.c_str(), test_ca_cert_pem_.c_str());

    std::string error;
    std::unique_ptr<base::Value> policy_value =
        base::JSONReader::ReadAndReturnError(
            policy_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
    ASSERT_TRUE(policy_value) << error;

    base::ListValue* policy = nullptr;
    ASSERT_TRUE(policy_value->GetAsList(&policy));

    std::string user_hash =
        onc_source == onc::ONC_SOURCE_USER_POLICY ? kUserHash : "";
    managed_config_handler_->SetPolicy(
        onc_source, user_hash, *policy,
        base::DictionaryValue() /* no global network config */);
  }

  void SetWifiState(const std::string& state) {
    ASSERT_TRUE(service_test_->SetServiceProperty(
        kWifiStub, shill::kStateProperty, base::Value(state)));
  }

  void GetServiceProperty(const std::string& prop_name,
                          std::string* prop_value) {
    prop_value->clear();
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kWifiStub);
    if (!properties)
      return;
    properties->GetStringWithoutPathExpansion(prop_name, prop_value);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  int network_properties_changed_count_ = 0;
  std::string test_cert_id_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  NetworkCertLoader* network_cert_loader_ = nullptr;

 private:
  // ClientCertResolver::Observer:
  void ResolveRequestCompleted(bool network_properties_changed) override {
    if (network_properties_changed)
      ++network_properties_changed_count_;
  }

  ShillServiceClient::TestInterface* service_test_ = nullptr;
  ShillProfileClient::TestInterface* profile_test_ = nullptr;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  net::ScopedCERTCertificate test_client_cert_;
  std::string test_ca_cert_pem_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB test_system_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertResolverTest);
};

TEST_F(ClientCertResolverTest, NoMatchingCertificates) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  StartNetworkCertLoader();
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();
  network_properties_changed_count_ = 0;
  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
}

TEST_F(ClientCertResolverTest, MatchIssuerCNWithoutIssuerInstalled) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerCN(onc::ONC_SOURCE_USER_POLICY);
  scoped_task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

// Test that matching works on a certificate with invalid characters in a
// PrintableString field. See crbug.com/788655.
TEST_F(ClientCertResolverTest, MatchSubjectOrgOnBadPrintableStringCert) {
  ASSERT_NO_FATAL_FAILURE(SetupTestCertWithBadPrintableString());

  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  ASSERT_NO_FATAL_FAILURE(
      SetupPolicyMatchingSubjectOrgForBadPrintableStringCert(
          onc::ONC_SOURCE_USER_POLICY));
  scoped_task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveOnCertificatesLoaded) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ResolveAfterPolicyApplication) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();
  StartNetworkCertLoader();
  SetupNetworkHandlers();
  scoped_task_environment_.RunUntilIdle();

  // Policy application will trigger the ClientCertResolver.
  network_properties_changed_count_ = 0;
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, ExpiringCertificate) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  SetWifiState(shill::kStateOnline);
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);

  // Verify that, after the certificate expired and the network disconnection
  // happens, no client certificate was configured and the ClientCertResolver
  // notified its observers with |network_properties_changed| = true.
  network_properties_changed_count_ = 0;
  test_clock_->SetNow(base::Time::Max());
  SetWifiState(shill::kStateOffline);
  scoped_task_environment_.RunUntilIdle();
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
}

// Test for crbug.com/765489. When the ClientCertResolver re-resolves a network
// as response to a NetworkConnectionStateChanged notification, and the
// resulting cert is the same as the last resolved cert, it should not call
// ResolveRequestCompleted with |network_properties_changed| = true.
TEST_F(ClientCertResolverTest, SameCertAfterNetworkConnectionStateChanged) {
  SetupTestCerts("client_1", true /* import issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  SetWifiState(shill::kStateOnline);
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);

  // Verify that after the network disconnection happens, the configured
  // certificate doesn't change and ClientCertResolver does not notify its
  // observers with |network_properties_changed| = true.
  network_properties_changed_count_ = 0;
  SetWifiState(shill::kStateOffline);
  scoped_task_environment_.RunUntilIdle();
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
  EXPECT_EQ(0, network_properties_changed_count_);
}

TEST_F(ClientCertResolverTest, UserPolicyUsesSystemToken) {
  SetupTestCertInSystemToken("client_1");
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerCN(onc::ONC_SOURCE_USER_POLICY);
  scoped_task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, network_cert_loader_->system_token_client_certs().size());

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, UserPolicyUsesSystemTokenSync) {
  SetupTestCertInSystemToken("client_1");
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(onc::ONC_SOURCE_USER_POLICY,
                                         &client_cert_config);

  base::DictionaryValue shill_properties;
  ClientCertResolver::ResolveCertificatePatternSync(
      client_cert::CONFIG_TYPE_EAP, client_cert_config, &shill_properties);
  std::string pkcs11_id;
  shill_properties.GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                 &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyUsesSystemToken) {
  SetupTestCertInSystemToken("client_1");
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerCN(onc::ONC_SOURCE_USER_POLICY);
  scoped_task_environment_.RunUntilIdle();

  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, network_cert_loader_->system_token_client_certs().size());

  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyUsesSystemTokenSync) {
  SetupTestCertInSystemToken("client_1");
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(onc::ONC_SOURCE_DEVICE_POLICY,
                                         &client_cert_config);

  base::DictionaryValue shill_properties;
  ClientCertResolver::ResolveCertificatePatternSync(
      client_cert::CONFIG_TYPE_EAP, client_cert_config, &shill_properties);
  std::string pkcs11_id;
  shill_properties.GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                 &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

TEST_F(ClientCertResolverTest, DevicePolicyDoesNotUseUserToken) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerCN(onc::ONC_SOURCE_DEVICE_POLICY);
  scoped_task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(0U, network_cert_loader_->system_token_client_certs().size());

  // Verify that no client certificate was configured.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
  EXPECT_EQ(1, network_properties_changed_count_);
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
}

TEST_F(ClientCertResolverTest, DevicePolicyDoesNotUseUserTokenSync) {
  SetupTestCerts("client_1", false /* do not import the issuer */);
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  client_cert::ClientCertConfig client_cert_config;
  SetupCertificateConfigMatchingIssuerCN(onc::ONC_SOURCE_DEVICE_POLICY,
                                         &client_cert_config);

  base::DictionaryValue shill_properties;
  ClientCertResolver::ResolveCertificatePatternSync(
      client_cert::CONFIG_TYPE_EAP, client_cert_config, &shill_properties);
  std::string pkcs11_id;
  shill_properties.GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                 &pkcs11_id);
  EXPECT_EQ(std::string(), pkcs11_id);
}

TEST_F(ClientCertResolverTest, PopulateIdentityFromCert) {
  SetupTestCerts("client_3", true /* import issuer */);
  SetupWifi();
  scoped_task_environment_.RunUntilIdle();

  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY,
                               "${CERT_SAN_EMAIL}");
  scoped_task_environment_.RunUntilIdle();

  network_properties_changed_count_ = 0;
  StartNetworkCertLoader();
  scoped_task_environment_.RunUntilIdle();

  // Verify that the resolver read the subjectAltName email field from the
  // cert, and wrote it into the shill service entry.
  std::string identity;
  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("santest@example.com", identity);
  EXPECT_EQ(1, network_properties_changed_count_);

  // Verify that after changing the ONC policy to request a variant of the
  // Microsoft Universal Principal Name field instead, the correct value is
  // substituted into the shill service entry.
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY,
                               "upn-${CERT_SAN_UPN}-suffix");
  scoped_task_environment_.RunUntilIdle();

  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("upn-santest@ad.corp.example.com-suffix", identity);
  EXPECT_EQ(2, network_properties_changed_count_);

  // Verify that after changing the ONC policy to request the subject CommonName
  // field, the correct value is substituted into the shill service entry.
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY,
                               "subject-cn-${CERT_SUBJECT_COMMON_NAME}-suffix");
  scoped_task_environment_.RunUntilIdle();

  GetServiceProperty(shill::kEapIdentityProperty, &identity);
  EXPECT_EQ("subject-cn-Client Cert F-suffix", identity);
  EXPECT_EQ(3, network_properties_changed_count_);
}

// Test for crbug.com/781276. A notification which results in no networks to be
// resolved should not alter the state of IsAnyResolveTaskRunning().
TEST_F(ClientCertResolverTest, TestResolveTaskQueued) {
  // Set up ClientCertResolver and let it run initially
  SetupTestCerts("client_1", true /* import issuer */);
  StartNetworkCertLoader();
  SetupWifi();
  SetupNetworkHandlers();
  SetupPolicyMatchingIssuerPEM(onc::ONC_SOURCE_USER_POLICY, "");
  scoped_task_environment_.RunUntilIdle();

  // Pretend that policy was applied, this shall queue a resolving task.
  static_cast<NetworkPolicyObserver*>(client_cert_resolver_.get())
      ->PolicyAppliedToNetwork(kWifiStub);
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Pretend that the network list has changed. One resolving task should still
  // be queued.
  static_cast<NetworkStateHandlerObserver*>(client_cert_resolver_.get())
      ->NetworkListChanged();
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Pretend that certificates have changed. One resolving task should still be
  // queued.
  static_cast<NetworkCertLoader::Observer*>(client_cert_resolver_.get())
      ->OnCertificatesLoaded(NetworkCertLoader::Get()->all_certs());
  EXPECT_TRUE(client_cert_resolver_->IsAnyResolveTaskRunning());

  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(client_cert_resolver_->IsAnyResolveTaskRunning());
  // Verify that the resolver positively matched the pattern in the policy with
  // the test client cert and configured the network.
  std::string pkcs11_id;
  GetServiceProperty(shill::kEapCertIdProperty, &pkcs11_id);
  EXPECT_EQ(test_cert_id_, pkcs11_id);
}

}  // namespace chromeos
