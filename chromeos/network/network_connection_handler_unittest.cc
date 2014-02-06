// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/tpm_token_loader.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char* kSuccessResult = "success";

void ConfigureCallback(const dbus::ObjectPath& result) {
}

void ConfigureErrorCallback(const std::string& error_name,
                            const std::string& error_message) {
}

}  // namespace

namespace chromeos {

class NetworkConnectionHandlerTest : public testing::Test {
 public:
  NetworkConnectionHandlerTest() : user_("userhash") {
  }
  virtual ~NetworkConnectionHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(user_.constructed_successfully());
    user_.FinishInit();

    test_nssdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    test_nssdb_->SetSlowTaskRunnerForTest(message_loop_.message_loop_proxy());

    TPMTokenLoader::InitializeForTest();

    CertLoader::Initialize();
    CertLoader* cert_loader = CertLoader::Get();
    cert_loader->force_hardware_backed_for_test();

    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    base::RunLoop().RunUntilIdle();
    DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()
        ->ClearServices();
    base::RunLoop().RunUntilIdle();
    LoginState::Initialize();
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get()));

    network_connection_handler_.reset(new NetworkConnectionHandler);
    network_connection_handler_->Init(network_state_handler_.get(),
                                      network_configuration_handler_.get());
  }

  virtual void TearDown() OVERRIDE {
    network_connection_handler_.reset();
    network_configuration_handler_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    TPMTokenLoader::Shutdown();
    LoginState::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  bool Configure(const std::string& json_string) {
    scoped_ptr<base::DictionaryValue> json_dict =
        onc::ReadDictionaryFromJson(json_string);
    if (!json_dict) {
      LOG(ERROR) << "Error parsing json: " << json_string;
      return false;
    }
    DBusThreadManager::Get()->GetShillManagerClient()->ConfigureService(
        *json_dict,
        base::Bind(&ConfigureCallback),
        base::Bind(&ConfigureErrorCallback));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void Connect(const std::string& service_path) {
    const bool check_error_state = true;
    network_connection_handler_->ConnectToNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerTest::ErrorCallback,
                   base::Unretained(this)),
        check_error_state);
    base::RunLoop().RunUntilIdle();
  }

  void Disconnect(const std::string& service_path) {
    network_connection_handler_->DisconnectNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerTest::ErrorCallback,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SuccessCallback() {
    result_ = kSuccessResult;
  }

  void ErrorCallback(const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
    result_ = error_name;
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key) {
    std::string result;
    const base::DictionaryValue* properties =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()->
        GetServiceProperties(service_path);
    if (properties)
      properties->GetStringWithoutPathExpansion(key, &result);
    return result;
  }

  void StartCertLoader() {
    CertLoader::Get()->StartWithNSSDB(test_nssdb_.get());
    base::RunLoop().RunUntilIdle();
  }

  void ImportClientCertAndKey(const std::string& pkcs12_file,
                              net::NSSCertDatabase* nssdb,
                              net::CertificateList* loaded_certs) {
    std::string pkcs12_data;
    base::FilePath pkcs12_path =
        net::GetTestCertsDirectory().Append(pkcs12_file);
    ASSERT_TRUE(base::ReadFileToString(pkcs12_path, &pkcs12_data));

    scoped_refptr<net::CryptoModule> module(
        net::CryptoModule::CreateFromHandle(nssdb->GetPrivateSlot().get()));
    ASSERT_EQ(
        net::OK,
        nssdb->ImportFromPKCS12(module, pkcs12_data, base::string16(), false,
                                loaded_certs));
    ASSERT_EQ(1U, loaded_certs->size());
  }

  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
  crypto::ScopedTestNSSChromeOSUser user_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_nssdb_;
  base::MessageLoopForUI message_loop_;
  std::string result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerTest);
};

namespace {

const char* kConfigConnectable =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
const char* kConfigConnected =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"online\" }";
const char* kConfigConnecting =
    "{ \"GUID\": \"wifi2\", \"Type\": \"wifi\", \"State\": \"association\" }";
const char* kConfigRequiresPassphrase =
    "{ \"GUID\": \"wifi3\", \"Type\": \"wifi\", "
    "  \"PassphraseRequired\": true }";
const char* kConfigRequiresActivation =
    "{ \"GUID\": \"cellular1\", \"Type\": \"cellular\","
    "  \"Cellular.ActivationState\": \"not-activated\" }";

}  // namespace

TEST_F(NetworkConnectionHandlerTest, NetworkConnectionHandlerConnectSuccess) {
  EXPECT_TRUE(Configure(kConfigConnectable));
  Connect("wifi0");
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
}

// Handles basic failure cases.
TEST_F(NetworkConnectionHandlerTest, NetworkConnectionHandlerConnectFailure) {
  Connect("no-network");
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigConnected));
  Connect("wifi1");
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected, GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigConnecting));
  Connect("wifi2");
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnecting, GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigRequiresPassphrase));
  Connect("wifi3");
  EXPECT_EQ(NetworkConnectionHandler::kErrorPassphraseRequired,
            GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigRequiresActivation));
  Connect("cellular1");
  EXPECT_EQ(NetworkConnectionHandler::kErrorActivationRequired,
            GetResultAndReset());
}

namespace {

const char* kConfigRequiresCertificateTemplate =
    "{ \"GUID\": \"wifi4\", \"Type\": \"wifi\", \"Connectable\": false,"
    "  \"Security\": \"802_1x\","
    "  \"UIData\": \"{"
    "    \\\"certificate_type\\\": \\\"pattern\\\","
    "    \\\"certificate_pattern\\\": {"
    "      \\\"Subject\\\": {\\\"CommonName\\\": \\\"%s\\\" }"
    "   } }\" }";

}  // namespace

// Handle certificates.
TEST_F(NetworkConnectionHandlerTest, ConnectCertificateMissing) {
  StartCertLoader();

  EXPECT_TRUE(Configure(
      base::StringPrintf(kConfigRequiresCertificateTemplate, "unknown")));
  Connect("wifi4");
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest, ConnectWithCertificateSuccess) {
  StartCertLoader();

  net::CertificateList certs;
  ImportClientCertAndKey("websocket_client_cert.p12",
                         test_nssdb_.get(),
                         &certs);

  EXPECT_TRUE(Configure(
      base::StringPrintf(kConfigRequiresCertificateTemplate,
                         certs[0]->subject().common_name.c_str())));

  Connect("wifi4");
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest,
       ConnectWithCertificateRequestedBeforeCertsAreLoaded) {
  net::CertificateList certs;
  ImportClientCertAndKey("websocket_client_cert.p12",
                         test_nssdb_.get(),
                         &certs);

  EXPECT_TRUE(Configure(
      base::StringPrintf(kConfigRequiresCertificateTemplate,
                         certs[0]->subject().common_name.c_str())));

  Connect("wifi4");

  // Connect request came before the cert loader loaded certificates, so the
  // connect request should have been throttled until the certificates are
  // loaded.
  EXPECT_EQ("", GetResultAndReset());

  StartCertLoader();

  // |StartCertLoader| should have triggered certificate loading.
  // When the certificates got loaded, the connection request should have
  // proceeded and eventually succeeded.
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest,
       NetworkConnectionHandlerDisconnectSuccess) {
  EXPECT_TRUE(Configure(kConfigConnected));
  Disconnect("wifi1");
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest,
       NetworkConnectionHandlerDisconnectFailure) {
  Connect("no-network");
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigConnectable));
  Disconnect("wifi0");
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());
}

}  // namespace chromeos
