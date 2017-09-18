// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/auto_connect_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/client_cert_resolver.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class TestCertResolveObserver : public ClientCertResolver::Observer {
 public:
  explicit TestCertResolveObserver(ClientCertResolver* cert_resolver)
      : changed_network_properties_(false), cert_resolver_(cert_resolver) {
    cert_resolver_->AddObserver(this);
  }

  void ResolveRequestCompleted(bool changed_network_properties) override {
    cert_resolver_->RemoveObserver(this);
    changed_network_properties_ = changed_network_properties;
  }

  bool DidNetworkPropertiesChange() { return changed_network_properties_; }

 private:
  bool changed_network_properties_;
  ClientCertResolver* cert_resolver_;
};

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  TestNetworkConnectionHandler(
      const base::Callback<void(const std::string&)>& disconnect_handler)
      : NetworkConnectionHandler(), disconnect_handler_(disconnect_handler) {}
  ~TestNetworkConnectionHandler() override {}

  // NetworkConnectionHandler:
  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override {
    disconnect_handler_.Run(service_path);
    success_callback.Run();
  }

  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state) override {}

  bool HasConnectingNetwork(const std::string& service_path) override {
    return false;
  }

  bool HasPendingConnectRequest() override { return false; }

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override {}

 private:
  base::Callback<void(const std::string&)> disconnect_handler_;
};

}  // namespace

class AutoConnectHandlerTest : public NetworkStateTest {
 public:
  AutoConnectHandlerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));

    CertLoader::Initialize();
    CertLoader::ForceHardwareBackedForTesting();

    DBusThreadManager::Initialize();

    NetworkStateTest::SetUp();

    LoginState::Initialize();

    network_config_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler(), nullptr /* network_device_handler */));

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(
        network_state_handler(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */,
        nullptr /* prohibited_technologies_handler */);

    test_network_connection_handler_.reset(
        new TestNetworkConnectionHandler(base::Bind(
            &AutoConnectHandlerTest::SetDisconnected, base::Unretained(this))));

    client_cert_resolver_.reset(new ClientCertResolver());
    client_cert_resolver_->Init(network_state_handler(),
                                managed_config_handler_.get());

    auto_connect_handler_.reset(new AutoConnectHandler());
    auto_connect_handler_->Init(
        client_cert_resolver_.get(), test_network_connection_handler_.get(),
        network_state_handler(), managed_config_handler_.get());

    scoped_task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    ShutdownNetworkState();
    auto_connect_handler_.reset();
    client_cert_resolver_.reset();
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_config_handler_.reset();

    LoginState::Shutdown();

    NetworkStateTest::TearDown();

    DBusThreadManager::Shutdown();
    CertLoader::Shutdown();
  }

 protected:
  void SetDisconnected(const std::string& service_path) {
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::Value(shill::kStateIdle));
  }

  std::string GetServiceState(const std::string& service_path) {
    return GetServiceStringProperty(service_path, shill::kStateProperty);
  }

  void StartCertLoader() {
    CertLoader::Get()->SetUserNSSDB(test_nsscertdb_.get());
    scoped_task_environment_.RunUntilIdle();
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    scoped_task_environment_.RunUntilIdle();
  }

  scoped_refptr<net::X509Certificate> ImportTestClientCert() {
    net::ScopedCERTCertificateList ca_cert_list =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), "client_1_ca.pem",
            net::X509Certificate::FORMAT_AUTO);
    if (ca_cert_list.empty()) {
      LOG(ERROR) << "No CA cert loaded.";
      return nullptr;
    }
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    if (!failures.empty()) {
      LOG(ERROR) << net::ErrorToString(failures[0].net_error);
      return nullptr;
    }

    // Import a client cert signed by that CA.
    scoped_refptr<net::X509Certificate> client_cert(
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            "client_1.pem", "client_1.pk8",
                                            test_nssdb_.slot()));
    return client_cert;
  }

  void SetupPolicy(const std::string& network_configs_json,
                   const base::DictionaryValue& global_config,
                   bool user_policy) {
    std::unique_ptr<base::ListValue> network_configs(new base::ListValue);
    if (!network_configs_json.empty()) {
      std::string error;
      std::unique_ptr<base::Value> network_configs_value =
          base::JSONReader::ReadAndReturnError(network_configs_json,
                                               base::JSON_ALLOW_TRAILING_COMMAS,
                                               nullptr, &error);
      ASSERT_TRUE(network_configs_value) << error;
      base::ListValue* network_configs_list = nullptr;
      ASSERT_TRUE(network_configs_value->GetAsList(&network_configs_list));
      ignore_result(network_configs_value.release());
      network_configs.reset(network_configs_list);
    }

    if (user_policy) {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
                                         kUserHash, *network_configs,
                                         global_config);
    } else {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         std::string(),  // no username hash
                                         *network_configs, global_config);
    }
    scoped_task_environment_.RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<AutoConnectHandler> auto_connect_handler_;
  std::unique_ptr<ClientCertResolver> client_cert_resolver_;
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoConnectHandlerTest);
};

namespace {

const char* kConfigUnmanagedSharedConnected =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"online\", "
    "  \"Security\": \"wpa\" }";
const char* kConfigManagedSharedConnectable =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Security\": \"wpa\" }";

const char* kPolicy =
    "[ { \"GUID\": \"wifi1\","
    "    \"Name\": \"wifi1\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-PSK\","
    "      \"HexSSID\": \"7769666931\","  // "wifi1"
    "      \"Passphrase\": \"passphrase\""
    "    }"
    "} ]";

const char* kPolicyCertPattern =
    "[ { \"GUID\": \"wifi1\","
    "    \"Name\": \"wifi1\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-EAP\","
    "      \"HexSSID\": \"7769666931\","  // "wifi1"
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
}  // namespace

TEST_F(AutoConnectHandlerTest, ReconnectOnCertLoading) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());
  test_manager_client()->SetBestServiceToConnect("wifi1");

  // User login shouldn't trigger any change until the certificates and policy
  // are loaded.
  LoginToRegularUser();
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      base::Value(true));

  SetupPolicy(std::string(),            // no network configs
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy
  SetupPolicy(kPolicy, global_config, false /* load as device policy */);
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  // Certificate loading should trigger connecting to the 'best' network.
  StartCertLoader();
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi1"));
}

TEST_F(AutoConnectHandlerTest, ReconnectOnCertPatternResolved) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());
  test_manager_client()->SetBestServiceToConnect("wifi0");

  SetupPolicy(std::string(),            // no device policy
              base::DictionaryValue(),  // no global config
              false);                   // load as device policy
  LoginToRegularUser();
  StartCertLoader();
  SetupPolicy(kPolicyCertPattern,
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  test_manager_client()->SetBestServiceToConnect("wifi1");
  TestCertResolveObserver observer(client_cert_resolver_.get());

  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer.DidNetworkPropertiesChange());

  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi1"));
}

// Ensure that resolving of certificate patterns only triggers a reconnect if at
// least one pattern was resolved.
TEST_F(AutoConnectHandlerTest, NoReconnectIfNoCertResolved) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());
  test_manager_client()->SetBestServiceToConnect("wifi0");

  SetupPolicy(std::string(),            // no device policy
              base::DictionaryValue(),  // no global config
              false);                   // load as device policy
  LoginToRegularUser();
  StartCertLoader();
  SetupPolicy(kPolicy,
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  test_manager_client()->SetBestServiceToConnect("wifi1");
  TestCertResolveObserver observer(client_cert_resolver_.get());
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer.DidNetworkPropertiesChange());

  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));
}

TEST_F(AutoConnectHandlerTest, DisconnectOnPolicyLoading) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      base::Value(true));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  // Because no best service is set, the fake implementation of
  // ConnectToBestServices will be a no-op.
  SetupPolicy(kPolicy, global_config, false /* load as device policy */);

  // Should not trigger any change until user policy is loaded
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  SetupPolicy(std::string(), base::DictionaryValue(), true);
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));
}

TEST_F(AutoConnectHandlerTest,
       DisconnectOnPolicyLoadingAllowOnlyPolicyNetworksToConnect) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  base::DictionaryValue global_config;
  global_config.SetKey(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
      base::Value(true));

  // Applying the policy which restricts autoconnect should disconnect from the
  // shared, unmanaged network.
  // Because no best service is set, the fake implementation of
  // ConnectToBestServices will be a no-op.
  SetupPolicy(kPolicy, global_config, false /* load as device policy */);

  // Should not trigger any change until user policy is loaded
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  SetupPolicy(std::string(), base::DictionaryValue(), true);
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));
}

// After login a reconnect is triggered even if there is no managed network.
TEST_F(AutoConnectHandlerTest, ReconnectAfterLogin) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());
  test_manager_client()->SetBestServiceToConnect("wifi1");

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartCertLoader();
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  // Applying an empty device policy will not trigger anything yet, until also
  // the user policy is applied.
  SetupPolicy(std::string(),            // no network configs
              base::DictionaryValue(),  // no global config
              false);                   // load as device policy
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  // Applying also an empty user policy should trigger connecting to the 'best'
  // network.
  SetupPolicy(std::string(),            // no network configs
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi1"));
}

TEST_F(AutoConnectHandlerTest, ManualConnectAbortsReconnectAfterLogin) {
  EXPECT_FALSE(ConfigureService(kConfigUnmanagedSharedConnected).empty());
  EXPECT_FALSE(ConfigureService(kConfigManagedSharedConnectable).empty());
  test_manager_client()->SetBestServiceToConnect("wifi1");

  // User login and certificate loading shouldn't trigger any change until the
  // policy is loaded.
  LoginToRegularUser();
  StartCertLoader();
  SetupPolicy(std::string(),            // no network configs
              base::DictionaryValue(),  // no global config
              false);                   // load as device policy

  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));

  // A manual connect request should prevent a reconnect after login.
  auto_connect_handler_->ConnectToNetworkRequested(
      std::string() /* service_path */);

  // Applying the user policy after login would usually trigger connecting to
  // the 'best' network. But the manual connect prevents this.
  SetupPolicy(std::string(),            // no network configs
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy
  EXPECT_EQ(shill::kStateOnline, GetServiceState("wifi0"));
  EXPECT_EQ(shill::kStateIdle, GetServiceState("wifi1"));
}

}  // namespace chromeos
