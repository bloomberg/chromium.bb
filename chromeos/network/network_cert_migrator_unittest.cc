// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/tpm_token_loader.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
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
const char* kEthernetEapStub = "ethernet_eap_stub";
const char* kVPNStub = "vpn_stub";
const char* kNSSNickname = "nss_nickname";
const char* kFakePEM = "pem";
const char* kProfile = "/profile/profile1";

}  // namespace

class NetworkCertMigratorTest : public testing::Test {
 public:
  NetworkCertMigratorTest() : service_test_(NULL),
                              user_("user_hash") {
  }
  virtual ~NetworkCertMigratorTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize NSS db for the user.
    ASSERT_TRUE(user_.constructed_successfully());
    user_.FinishInit();
    test_nssdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    test_nssdb_->SetSlowTaskRunnerForTest(message_loop_.message_loop_proxy());

    DBusThreadManager::InitializeWithStub();
    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    DBusThreadManager::Get()
        ->GetShillProfileClient()
        ->GetTestInterface()
        ->AddProfile(kProfile, "" /* userhash */);
    base::RunLoop().RunUntilIdle();
    service_test_->ClearServices();
    base::RunLoop().RunUntilIdle();

    CertLoader::Initialize();
    CertLoader* cert_loader_ = CertLoader::Get();
    cert_loader_->StartWithNSSDB(test_nssdb_.get());
  }

  virtual void TearDown() OVERRIDE {
    network_cert_migrator_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    DBusThreadManager::Shutdown();
    CleanupTestCert();
  }

 protected:
  void SetupTestCACert() {
    scoped_refptr<net::X509Certificate> cert_wo_nickname =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "eku-test-root.pem",
                                           net::X509Certificate::FORMAT_AUTO)
            .back();
    net::X509Certificate::GetPEMEncoded(cert_wo_nickname->os_cert_handle(),
                                        &test_ca_cert_pem_);
    std::string der_encoded;
    net::X509Certificate::GetDEREncoded(cert_wo_nickname->os_cert_handle(),
                                        &der_encoded);
    cert_wo_nickname = NULL;

    test_ca_cert_ = net::X509Certificate::CreateFromBytesWithNickname(
        der_encoded.data(), der_encoded.size(), kNSSNickname);
    net::CertificateList cert_list;
    cert_list.push_back(test_ca_cert_);
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nssdb_->ImportCACerts(
        cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    ASSERT_TRUE(failures.empty()) << net::ErrorToString(failures[0].net_error);
  }

  void SetupTestClientCert() {
    std::string pkcs12_data;
    ASSERT_TRUE(base::ReadFileToString(
        net::GetTestCertsDirectory().Append("websocket_client_cert.p12"),
        &pkcs12_data));

    net::CertificateList client_cert_list;
    scoped_refptr<net::CryptoModule> module(net::CryptoModule::CreateFromHandle(
        test_nssdb_->GetPrivateSlot().get()));
    ASSERT_EQ(
        net::OK,
        test_nssdb_->ImportFromPKCS12(
            module, pkcs12_data, base::string16(), false, &client_cert_list));
    ASSERT_TRUE(!client_cert_list.empty());
    test_client_cert_ = client_cert_list[0];

    int slot_id = -1;
    test_client_cert_pkcs11_id_ = CertLoader::GetPkcs11IdAndSlotForCert(
        *test_client_cert_, &slot_id);
    ASSERT_FALSE(test_client_cert_pkcs11_id_.empty());
    ASSERT_NE(-1, slot_id);
    test_client_cert_slot_id_ = base::IntToString(slot_id);
  }

  void SetupNetworkHandlers() {
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_cert_migrator_.reset(new NetworkCertMigrator);
    network_cert_migrator_->Init(network_state_handler_.get());
  }

  void AddService(const std::string& network_id,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(network_id /* service_path */,
                              network_id /* guid */,
                              network_id /* name */,
                              type,
                              state,
                              true /* add_to_visible */);

    // Ensure that the service appears as 'configured', i.e. is associated to a
    // Shill profile.
    service_test_->SetServiceProperty(
        network_id, shill::kProfileProperty, base::StringValue(kProfile));
  }

  void SetupWifiWithNss() {
    AddService(kWifiStub, shill::kTypeWifi, shill::kStateOnline);
    service_test_->SetServiceProperty(kWifiStub,
                                      shill::kEapCaCertNssProperty,
                                      base::StringValue(kNSSNickname));
  }

  void SetupNetworkWithEapCertId(bool wifi, const std::string& cert_id) {
    std::string type = wifi ? shill::kTypeWifi: shill::kTypeEthernetEap;
    std::string name = wifi ? kWifiStub : kEthernetEapStub;
    AddService(name, type, shill::kStateOnline);
    service_test_->SetServiceProperty(
        name, shill::kEapCertIdProperty, base::StringValue(cert_id));
    service_test_->SetServiceProperty(
        name, shill::kEapKeyIdProperty, base::StringValue(cert_id));

    if (wifi) {
      service_test_->SetServiceProperty(
          name,
          shill::kSecurityProperty,
          base::StringValue(shill::kSecurity8021x));
    }
  }

  void GetEapCertId(bool wifi, std::string* cert_id) {
    cert_id->clear();

    std::string name = wifi ? kWifiStub : kEthernetEapStub;
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(name);
    properties->GetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                              cert_id);
  }

  void SetupVpnWithCertId(bool open_vpn,
                          const std::string& slot_id,
                          const std::string& pkcs11_id) {
    AddService(kVPNStub, shill::kTypeVPN, shill::kStateIdle);
    base::DictionaryValue provider;
    if (open_vpn) {
      provider.SetStringWithoutPathExpansion(shill::kTypeProperty,
                                             shill::kProviderOpenVpn);
      provider.SetStringWithoutPathExpansion(
          shill::kOpenVPNClientCertIdProperty, pkcs11_id);
    } else {
      provider.SetStringWithoutPathExpansion(shill::kTypeProperty,
                                             shill::kProviderL2tpIpsec);
      provider.SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, slot_id);
      provider.SetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, pkcs11_id);
    }
    service_test_->SetServiceProperty(
        kVPNStub, shill::kProviderProperty, provider);
  }

  void GetVpnCertId(bool open_vpn,
                    std::string* slot_id,
                    std::string* pkcs11_id) {
    slot_id->clear();
    pkcs11_id->clear();

    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kVPNStub);
    ASSERT_TRUE(properties);
    const base::DictionaryValue* provider = NULL;
    properties->GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                                  &provider);
    if (!provider)
      return;
    if (open_vpn) {
      provider->GetStringWithoutPathExpansion(
          shill::kOpenVPNClientCertIdProperty, pkcs11_id);
    } else {
      provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertSlotProperty, slot_id);
      provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, pkcs11_id);
    }
  }

  void GetEapCACertProperties(std::string* nss_nickname, std::string* ca_pem) {
    nss_nickname->clear();
    ca_pem->clear();
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kWifiStub);
    properties->GetStringWithoutPathExpansion(shill::kEapCaCertNssProperty,
                                              nss_nickname);
    const base::ListValue* ca_pems = NULL;
    properties->GetListWithoutPathExpansion(shill::kEapCaCertPemProperty,
                                            &ca_pems);
    if (ca_pems && !ca_pems->empty())
      ca_pems->GetString(0, ca_pem);
  }

  void SetupVpnWithNss(bool open_vpn) {
    AddService(kVPNStub, shill::kTypeVPN, shill::kStateIdle);
    base::DictionaryValue provider;
    const char* nss_property = open_vpn ? shill::kOpenVPNCaCertNSSProperty
                                        : shill::kL2tpIpsecCaCertNssProperty;
    provider.SetStringWithoutPathExpansion(nss_property, kNSSNickname);
    service_test_->SetServiceProperty(
        kVPNStub, shill::kProviderProperty, provider);
  }

  void GetVpnCACertProperties(bool open_vpn,
                              std::string* nss_nickname,
                              std::string* ca_pem) {
    nss_nickname->clear();
    ca_pem->clear();
    const base::DictionaryValue* properties =
        service_test_->GetServiceProperties(kVPNStub);
    const base::DictionaryValue* provider = NULL;
    properties->GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                                  &provider);
    if (!provider)
      return;
    const char* nss_property = open_vpn ? shill::kOpenVPNCaCertNSSProperty
                                        : shill::kL2tpIpsecCaCertNssProperty;
    provider->GetStringWithoutPathExpansion(nss_property, nss_nickname);
    const base::ListValue* ca_pems = NULL;
    const char* pem_property = open_vpn ? shill::kOpenVPNCaCertPemProperty
                                        : shill::kL2tpIpsecCaCertPemProperty;
    provider->GetListWithoutPathExpansion(pem_property, &ca_pems);
    if (ca_pems && !ca_pems->empty())
      ca_pems->GetString(0, ca_pem);
  }

  ShillServiceClient::TestInterface* service_test_;
  scoped_refptr<net::X509Certificate> test_ca_cert_;
  scoped_refptr<net::X509Certificate> test_client_cert_;
  std::string test_client_cert_pkcs11_id_;
  std::string test_client_cert_slot_id_;
  std::string test_ca_cert_pem_;
  base::MessageLoop message_loop_;

 private:
  void CleanupTestCert() {
    if (test_ca_cert_)
      ASSERT_TRUE(test_nssdb_->DeleteCertAndKey(test_ca_cert_.get()));

    if (test_client_cert_)
      ASSERT_TRUE(test_nssdb_->DeleteCertAndKey(test_client_cert_.get()));
  }

  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkCertMigrator> network_cert_migrator_;
  crypto::ScopedTestNSSChromeOSUser user_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_nssdb_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertMigratorTest);
};

TEST_F(NetworkCertMigratorTest, MigrateNssOnInitialization) {
  // Add a new network for migration before the handlers are initialized.
  SetupWifiWithNss();
  SetupTestCACert();
  SetupNetworkHandlers();

  base::RunLoop().RunUntilIdle();
  std::string nss_nickname, ca_pem;
  GetEapCACertProperties(&nss_nickname, &ca_pem);
  EXPECT_TRUE(nss_nickname.empty());
  EXPECT_EQ(test_ca_cert_pem_, ca_pem);
}

TEST_F(NetworkCertMigratorTest, MigrateNssOnNetworkAppearance) {
  SetupTestCACert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupWifiWithNss();

  base::RunLoop().RunUntilIdle();
  std::string nss_nickname, ca_pem;
  GetEapCACertProperties(&nss_nickname, &ca_pem);
  EXPECT_TRUE(nss_nickname.empty());
  EXPECT_EQ(test_ca_cert_pem_, ca_pem);
}

TEST_F(NetworkCertMigratorTest, DoNotMigrateNssIfPemSet) {
  // Add a new network with an already set PEM property.
  SetupWifiWithNss();
  base::ListValue ca_pems;
  ca_pems.AppendString(kFakePEM);
  service_test_->SetServiceProperty(
      kWifiStub, shill::kEapCaCertPemProperty, ca_pems);

  SetupTestCACert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  std::string nss_nickname, ca_pem;
  GetEapCACertProperties(&nss_nickname, &ca_pem);
  EXPECT_TRUE(nss_nickname.empty());
  EXPECT_EQ(kFakePEM, ca_pem);
}

TEST_F(NetworkCertMigratorTest, MigrateNssOpenVpn) {
  // Add a new network for migration before the handlers are initialized.
  SetupVpnWithNss(true /* OpenVPN */);

  SetupTestCACert();
  SetupNetworkHandlers();

  base::RunLoop().RunUntilIdle();
  std::string nss_nickname, ca_pem;
  GetVpnCACertProperties(true /* OpenVPN */, &nss_nickname, &ca_pem);
  EXPECT_TRUE(nss_nickname.empty());
  EXPECT_EQ(test_ca_cert_pem_, ca_pem);
}

TEST_F(NetworkCertMigratorTest, MigrateNssIpsecVpn) {
  // Add a new network for migration before the handlers are initialized.
  SetupVpnWithNss(false /* not OpenVPN */);

  SetupTestCACert();
  SetupNetworkHandlers();

  base::RunLoop().RunUntilIdle();
  std::string nss_nickname, ca_pem;
  GetVpnCACertProperties(false /* not OpenVPN */, &nss_nickname, &ca_pem);
  EXPECT_TRUE(nss_nickname.empty());
  EXPECT_EQ(test_ca_cert_pem_, ca_pem);
}

TEST_F(NetworkCertMigratorTest, MigrateEapCertIdNoMatchingCert) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(true /* wifi */, "unknown pkcs11 id");

  base::RunLoop().RunUntilIdle();
  // Since the PKCS11 ID is unknown, the certificate configuration will be
  // cleared.
  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(std::string(), cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateEapCertIdNoSlotId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(true /* wifi */, test_client_cert_pkcs11_id_);

  base::RunLoop().RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateWifiEapCertIdWrongSlotId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(true /* wifi */,
                            "123:" + test_client_cert_pkcs11_id_);

  base::RunLoop().RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, DoNotChangeEapCertIdWithCorrectSlotId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(true /* wifi */, expected_cert_id);

  base::RunLoop().RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, IgnoreOpenVPNCertId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  const char kPkcs11Id[] = "any slot id";

  // Add a new network for migration after the handlers are initialized.
  SetupVpnWithCertId(
      true /* OpenVPN */, std::string() /* no slot id */, kPkcs11Id);

  base::RunLoop().RunUntilIdle();

  std::string pkcs11_id;
  std::string unused_slot_id;
  GetVpnCertId(true /* OpenVPN */, &unused_slot_id, &pkcs11_id);
  EXPECT_EQ(kPkcs11Id, pkcs11_id);
}

TEST_F(NetworkCertMigratorTest, MigrateEthernetEapCertIdWrongSlotId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupNetworkWithEapCertId(
      false /* ethernet */, "123:" + test_client_cert_pkcs11_id_);

  base::RunLoop().RunUntilIdle();

  std::string cert_id;
  GetEapCertId(false /* ethernet */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
}

TEST_F(NetworkCertMigratorTest, MigrateIpsecCertIdWrongSlotId) {
  SetupTestClientCert();
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  // Add a new network for migration after the handlers are initialized.
  SetupVpnWithCertId(false /* IPsec */, "123", test_client_cert_pkcs11_id_);

  base::RunLoop().RunUntilIdle();

  std::string pkcs11_id;
  std::string slot_id;
  GetVpnCertId(false /* IPsec */, &slot_id, &pkcs11_id);
  EXPECT_EQ(test_client_cert_pkcs11_id_, pkcs11_id);
  EXPECT_EQ(test_client_cert_slot_id_, slot_id);
}

}  // namespace chromeos
