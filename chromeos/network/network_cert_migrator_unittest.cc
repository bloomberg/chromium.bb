// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>
#include <pk11pub.h>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state_handler.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kWifiStub = "wifi_stub";
const char* kEthernetEapStub = "ethernet_eap_stub";
const char* kVPNStub = "vpn_stub";
const char* kProfile = "/profile/profile1";

}  // namespace

class NetworkCertMigratorTest : public testing::Test {
 public:
  NetworkCertMigratorTest() : service_test_(nullptr) {}
  ~NetworkCertMigratorTest() override {}

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());
    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));
    test_nsscertdb_->SetSlowTaskRunnerForTest(message_loop_.task_runner());

    DBusThreadManager::Initialize();
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
    cert_loader_->StartWithNSSDB(test_nsscertdb_.get());
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_cert_migrator_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  void SetupTestClientCert() {
    test_client_cert_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
        test_nssdb_.slot());
    ASSERT_TRUE(test_client_cert_.get());

    int slot_id = -1;
    test_client_cert_pkcs11_id_ = CertLoader::GetPkcs11IdAndSlotForCert(
        *test_client_cert_, &slot_id);
    ASSERT_FALSE(test_client_cert_pkcs11_id_.empty());
    ASSERT_NE(-1, slot_id);
    test_client_cert_slot_id_ = base::IntToString(slot_id);
  }

  void SetupNetworkHandlers() {
    network_state_handler_ = NetworkStateHandler::InitializeForTest();
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
          shill::kSecurityClassProperty,
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
    const base::DictionaryValue* provider = nullptr;
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

  ShillServiceClient::TestInterface* service_test_;
  scoped_refptr<net::X509Certificate> test_client_cert_;
  std::string test_client_cert_pkcs11_id_;
  std::string test_client_cert_slot_id_;
  base::MessageLoop message_loop_;

 private:
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkCertMigrator> network_cert_migrator_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertMigratorTest);
};

TEST_F(NetworkCertMigratorTest, MigrateOnInitialization) {
  SetupTestClientCert();
  // Add a network for migration before the handlers are initialized.
  SetupNetworkWithEapCertId(true /* wifi */,
                            "123:" + test_client_cert_pkcs11_id_);
  SetupNetworkHandlers();
  base::RunLoop().RunUntilIdle();

  std::string cert_id;
  GetEapCertId(true /* wifi */, &cert_id);
  std::string expected_cert_id =
      test_client_cert_slot_id_ + ":" + test_client_cert_pkcs11_id_;
  EXPECT_EQ(expected_cert_id, cert_id);
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
