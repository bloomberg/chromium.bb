// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/tpm_token_loader.h"
#include "components/onc/onc_constants.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
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
  NetworkConnectionHandlerTest()
      : user_("userhash"),
        test_manager_client_(NULL),
        test_service_client_(NULL) {}

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

    FakeDBusThreadManager* dbus_manager = new FakeDBusThreadManager;
    dbus_manager->SetFakeClients();
    DBusThreadManager::InitializeForTesting(dbus_manager);
    test_manager_client_ =
        dbus_manager->GetShillManagerClient()->GetTestInterface();
    test_service_client_ =
        dbus_manager->GetShillServiceClient()->GetTestInterface();

    test_manager_client_->AddTechnology(shill::kTypeWifi, true /* enabled */);
    dbus_manager->GetShillDeviceClient()->GetTestInterface()->AddDevice(
        "/device/wifi1", shill::kTypeWifi, "wifi_device1");
    test_manager_client_->AddTechnology(shill::kTypeCellular,
                                        true /* enabled */);
    dbus_manager->GetShillProfileClient()->GetTestInterface()->AddProfile(
        "shared_profile_path", std::string() /* shared profile */);
    dbus_manager->GetShillProfileClient()->GetTestInterface()->AddProfile(
        "user_profile_path", user_.username_hash());

    base::RunLoop().RunUntilIdle();
    LoginState::Initialize();
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_config_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get()));

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(network_state_handler_.get(),
                                  network_profile_handler_.get(),
                                  network_config_handler_.get(),
                                  NULL /* network_device_handler */);

    network_connection_handler_.reset(new NetworkConnectionHandler);
    network_connection_handler_->Init(network_state_handler_.get(),
                                      network_config_handler_.get(),
                                      managed_config_handler_.get());

    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_connection_handler_.reset();
    network_config_handler_.reset();
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
        test_service_client_->GetServiceProperties(service_path);
    if (properties)
      properties->GetStringWithoutPathExpansion(key, &result);
    return result;
  }

  void StartCertLoader() {
    CertLoader::Get()->StartWithNSSDB(test_nssdb_.get());
    base::RunLoop().RunUntilIdle();
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<net::X509Certificate> ImportTestClientCert() {
    net::CertificateList ca_cert_list =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "websocket_cacert.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    if (ca_cert_list.empty()) {
      LOG(ERROR) << "No CA cert loaded.";
      return NULL;
    }
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nssdb_->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    if (!failures.empty()) {
      LOG(ERROR) << net::ErrorToString(failures[0].net_error);
      return NULL;
    }

    std::string pkcs12_data;
    base::FilePath pkcs12_path =
        net::GetTestCertsDirectory().Append("websocket_client_cert.p12");
    if (!base::ReadFileToString(pkcs12_path, &pkcs12_data))
      return NULL;

    net::CertificateList loaded_certs;
    scoped_refptr<net::CryptoModule> module(net::CryptoModule::CreateFromHandle(
        test_nssdb_->GetPrivateSlot().get()));
    if (test_nssdb_->ImportFromPKCS12(
            module, pkcs12_data, base::string16(), false, &loaded_certs) !=
        net::OK) {
      LOG(ERROR) << "Error while importing to NSSDB.";
      return NULL;
    }

    // File contains two certs, the client cert first and the CA cert second.
    if (loaded_certs.size() != 2U) {
      LOG(ERROR) << "Expected two certs in file, found " << loaded_certs.size();
      return NULL;
    }
    return loaded_certs[0];
  }

  void SetupPolicy(const std::string& network_configs_json,
                   const base::DictionaryValue& global_config,
                   bool user_policy) {
    std::string error;
    scoped_ptr<base::Value> network_configs_value(
        base::JSONReader::ReadAndReturnError(network_configs_json,
                                             base::JSON_ALLOW_TRAILING_COMMAS,
                                             NULL,
                                             &error));
    ASSERT_TRUE(network_configs_value) << error;

    base::ListValue* network_configs = NULL;
    ASSERT_TRUE(network_configs_value->GetAsList(&network_configs));

    if (user_policy) {
      managed_config_handler_->SetPolicy(
          ::onc::ONC_SOURCE_USER_POLICY,
          user_.username_hash(),
          *network_configs,
          global_config);
    } else {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         std::string(),  // no username hash
                                         *network_configs,
                                         global_config);
    }
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_config_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl> managed_config_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  crypto::ScopedTestNSSChromeOSUser user_;
  ShillManagerClient::TestInterface* test_manager_client_;
  ShillServiceClient::TestInterface* test_service_client_;
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
}

namespace {

const char* kPolicyWithCertPatternTemplate =
    "[ { \"GUID\": \"wifi4\","
    "    \"Name\": \"wifi4\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-EAP\","
    "      \"SSID\": \"wifi_ssid\","
    "      \"EAP\": {"
    "        \"Outer\": \"EAP-TLS\","
    "        \"ClientCertType\": \"Pattern\","
    "        \"ClientCertPattern\": {"
    "          \"Subject\": {"
    "            \"CommonName\" : \"%s\""
    "          }"
    "        }"
    "      }"
    "    }"
    "} ]";

}  // namespace

// Handle certificates.
TEST_F(NetworkConnectionHandlerTest, ConnectCertificateMissing) {
  StartCertLoader();
  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate, "unknown"),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect("wifi4");
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest, ConnectWithCertificateSuccess) {
  StartCertLoader();
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert);

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect("wifi4");
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

// Disabled, see http://crbug.com/396729.
TEST_F(NetworkConnectionHandlerTest,
       DISABLED_ConnectWithCertificateRequestedBeforeCertsAreLoaded) {
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert);

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

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

namespace {

const char* kConfigUnmanagedSharedConnected =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"online\" }";
const char* kConfigManagedSharedConnectable =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";

const char* kPolicy =
    "[ { \"GUID\": \"wifi1\","
    "    \"Name\": \"wifi1\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-PSK\","
    "      \"SSID\": \"wifi1\","
    "      \"Passphrase\": \"passphrase\""
    "    }"
    "} ]";

}  // namespace

TEST_F(NetworkConnectionHandlerTest, ReconnectOnLoginEarlyPolicyLoading) {
  EXPECT_TRUE(Configure(kConfigUnmanagedSharedConnected));
  EXPECT_TRUE(Configure(kConfigManagedSharedConnectable));
  test_manager_client_->SetBestServiceToConnect("wifi1");

  // User login shouldn't trigger any change because policy is not loaded yet.
  LoginToRegularUser();
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi1", shill::kStateProperty));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  base::DictionaryValue global_config;
  global_config.SetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);

  SetupPolicy(kPolicy, global_config, false /* load as device policy */);
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi1", shill::kStateProperty));

  // Certificate loading should trigger connecting to the 'best' network.
  StartCertLoader();
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty("wifi1", shill::kStateProperty));
}

TEST_F(NetworkConnectionHandlerTest, ReconnectOnLoginLatePolicyLoading) {
  EXPECT_TRUE(Configure(kConfigUnmanagedSharedConnected));
  EXPECT_TRUE(Configure(kConfigManagedSharedConnectable));
  test_manager_client_->SetBestServiceToConnect("wifi1");

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartCertLoader();
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi1", shill::kStateProperty));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  base::DictionaryValue global_config;
  global_config.SetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      true);

  SetupPolicy(kPolicy, global_config, false /* load as device policy */);
  EXPECT_EQ(shill::kStateIdle,
            GetServiceStringProperty("wifi0", shill::kStateProperty));
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty("wifi1", shill::kStateProperty));
}

}  // namespace chromeos
