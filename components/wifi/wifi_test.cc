// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/wifi/wifi_service.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace wifi {

class WiFiTest {
 public:
  WiFiTest() {}
  ~WiFiTest() {}

  enum Result {
    RESULT_ERROR = -2,
    RESULT_WRONG_USAGE = -1,
    RESULT_OK = 0,
    RESULT_PENDING = 1,
  };

  Result Main(int argc, const char* argv[]);

 private:
  bool ParseCommandLine(int argc, const char* argv[]);

  void Start() {}
  void Finish(Result result) {
    DCHECK_NE(RESULT_PENDING, result);
    result_ = result;
    if (base::MessageLoop::current())
      base::MessageLoop::current()->Quit();
  }

  void OnNetworksChanged(
      const WiFiService::NetworkGuidList& network_guid_list) {
    VLOG(0) << "Networks Changed: " << network_guid_list[0];
    base::DictionaryValue properties;
    std::string error;
    wifi_service_->GetProperties(network_guid_list[0], &properties, &error);
    VLOG(0) << error << ":\n" << properties;
  }

  void OnNetworkListChanged(
      const WiFiService::NetworkGuidList& network_guid_list) {
    VLOG(0) << "Network List Changed: " << network_guid_list.size();
  }

#if defined(OS_MACOSX)
  // Without this there will be a mem leak on osx.
  base::mac::ScopedNSAutoreleasePool scoped_pool_;
#endif

  scoped_ptr<WiFiService> wifi_service_;

  // Need AtExitManager to support AsWeakPtr (in NetLog).
  base::AtExitManager exit_manager_;

  Result result_;
};

WiFiTest::Result WiFiTest::Main(int argc, const char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
    VLOG(0) <<  "Usage: " << argv[0] <<
                " [--list]"
                " [--get_key]"
                " [--get_properties]"
                " [--create]"
                " [--connect]"
                " [--disconnect]"
                " [--scan]"
                " [--network_guid=<network_guid>]"
                " [--frequency=0|2400|5000]"
                " [--security=none|WEP-PSK|WPA-PSK|WPA2-PSK]"
                " [--password=<wifi_password>]"
                " [<network_guid>]\n";
    return RESULT_WRONG_USAGE;
  }

  base::MessageLoopForIO loop;
  result_ = RESULT_PENDING;

  return result_;
}

bool WiFiTest::ParseCommandLine(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::string network_guid =
      parsed_command_line.GetSwitchValueASCII("network_guid");
  std::string frequency =
      parsed_command_line.GetSwitchValueASCII("frequency");
  std::string password =
      parsed_command_line.GetSwitchValueASCII("password");
  std::string security =
      parsed_command_line.GetSwitchValueASCII("security");

  if (parsed_command_line.GetArgs().size() == 1) {
#if defined(OS_WIN)
    network_guid = base::UTF16ToASCII(parsed_command_line.GetArgs()[0]);
#else
    network_guid = parsed_command_line.GetArgs()[0];
#endif
  }

#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch("debug"))
    MessageBoxA(NULL, __FUNCTION__, "Debug Me!", MB_OK);
#endif

  base::MessageLoopForIO loop;

  wifi_service_.reset(WiFiService::Create());
  wifi_service_->Initialize(loop.message_loop_proxy());

  if (parsed_command_line.HasSwitch("list")) {
    base::ListValue network_list;
    wifi_service_->GetVisibleNetworks(std::string(), &network_list, true);
    VLOG(0) << network_list;
    return true;
  }

  if (parsed_command_line.HasSwitch("get_properties")) {
    if (network_guid.length() > 0) {
      base::DictionaryValue properties;
      std::string error;
      wifi_service_->GetProperties(network_guid, &properties, &error);
      VLOG(0) << error << ":\n" << properties;
      return true;
    }
  }

  // Optional properties (frequency, password) to use for connect or create.
  scoped_ptr<base::DictionaryValue> properties(new base::DictionaryValue());

  if (!frequency.empty()) {
    int value = 0;
    if (base::StringToInt(frequency, &value)) {
      properties->SetInteger("WiFi.Frequency", value);
      // fall through to connect.
    }
  }

  if (!password.empty())
    properties->SetString("WiFi.Passphrase", password);

  if (!security.empty())
    properties->SetString("WiFi.Security", security);

  if (parsed_command_line.HasSwitch("create")) {
    if (!network_guid.empty()) {
      std::string error;
      std::string new_network_guid;
      properties->SetString("WiFi.SSID", network_guid);
      VLOG(0) << "Creating Network: " << *properties;
      wifi_service_->CreateNetwork(
          false, properties.Pass(), &new_network_guid, &error);
      VLOG(0) << error << ":\n" << new_network_guid;
      return true;
    }
  }

  if (parsed_command_line.HasSwitch("connect")) {
    if (!network_guid.empty()) {
      std::string error;
      if (!properties->empty()) {
        VLOG(0) << "Using connect properties: " << *properties;
        wifi_service_->SetProperties(network_guid, properties.Pass(), &error);
      }

      wifi_service_->SetEventObservers(
          loop.message_loop_proxy(),
          base::Bind(&WiFiTest::OnNetworksChanged, base::Unretained(this)),
          base::Bind(&WiFiTest::OnNetworkListChanged, base::Unretained(this)));

      wifi_service_->StartConnect(network_guid, &error);
      VLOG(0) << error;
      if (error.empty())
        base::MessageLoop::current()->Run();
      return true;
    }
  }

  if (parsed_command_line.HasSwitch("disconnect")) {
    if (network_guid.length() > 0) {
      std::string error;
      wifi_service_->StartDisconnect(network_guid, &error);
      VLOG(0) << error;
      return true;
    }
  }

  if (parsed_command_line.HasSwitch("get_key")) {
    if (network_guid.length() > 0) {
      std::string error;
      std::string key_data;
      wifi_service_->GetKeyFromSystem(network_guid, &key_data, &error);
      VLOG(0) << key_data << error;
      return true;
    }
  }

  if (parsed_command_line.HasSwitch("scan")) {
    wifi_service_->SetEventObservers(
        loop.message_loop_proxy(),
        base::Bind(&WiFiTest::OnNetworksChanged, base::Unretained(this)),
        base::Bind(&WiFiTest::OnNetworkListChanged, base::Unretained(this)));
    wifi_service_->RequestNetworkScan();
    base::MessageLoop::current()->Run();
    return true;
  }

  return false;
}

}  // namespace wifi

int main(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  wifi::WiFiTest wifi_test;
  return wifi_test.Main(argc, argv);
}
