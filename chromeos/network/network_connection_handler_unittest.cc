// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
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
  NetworkConnectionHandlerTest() {
  }
  virtual ~NetworkConnectionHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    message_loop_.RunUntilIdle();
    DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()
        ->ClearServices();
    message_loop_.RunUntilIdle();
    LoginState::Initialize();
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get()));
    network_connection_handler_.reset(new NetworkConnectionHandler);
    // TODO(stevenjb): Test integration with CertLoader using a stub or mock.
    network_connection_handler_->Init(network_state_handler_.get(),
                                      network_configuration_handler_.get());
  }

  virtual void TearDown() OVERRIDE {
    network_connection_handler_.reset();
    network_configuration_handler_.reset();
    network_state_handler_.reset();
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
    message_loop_.RunUntilIdle();
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
    message_loop_.RunUntilIdle();
  }

  void Disconnect(const std::string& service_path) {
    network_connection_handler_->DisconnectNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerTest::ErrorCallback,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
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

  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
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
  EXPECT_EQ(flimflam::kStateOnline,
            GetServiceStringProperty("wifi0", flimflam::kStateProperty));
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

const char* kConfigRequiresCertificate =
    "{ \"GUID\": \"wifi4\", \"Type\": \"wifi\", \"Connectable\": false,"
    "  \"Security\": \"802_1x\","
    "  \"UIData\": \"{"
    "    \\\"certificate_type\\\": \\\"pattern\\\","
    "    \\\"certificate_pattern\\\": {"
    "      \\\"Subject\\\": { \\\"CommonName\\\": \\\"Foo\\\" }"
    "   } }\" }";

}  // namespace

// Handle certificates. TODO(stevenjb): Add certificate stubs to improve
// test coverage.
TEST_F(NetworkConnectionHandlerTest,
       NetworkConnectionHandlerConnectCertificate) {
  EXPECT_TRUE(Configure(kConfigRequiresCertificate));
  Connect("wifi4");
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
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
