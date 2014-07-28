// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
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
const char* kVPNStub = "vpn_stub";
const char* kNSSNickname = "nss_nickname";
const char* kFakePEM = "pem";

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
  }

  void SetupWifiWithNss() {
    AddService(kWifiStub, shill::kTypeWifi, shill::kStateOnline);
    service_test_->SetServiceProperty(kWifiStub,
                                      shill::kEapCaCertNssProperty,
                                      base::StringValue(kNSSNickname));
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
  std::string test_ca_cert_pem_;
  base::MessageLoop message_loop_;

 private:
  void CleanupTestCert() {
    ASSERT_TRUE(test_nssdb_->DeleteCertAndKey(test_ca_cert_.get()));
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

TEST_F(NetworkCertMigratorTest, MigrateOpenVpn) {
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

TEST_F(NetworkCertMigratorTest, MigrateIpsecVpn) {
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

}  // namespace chromeos
