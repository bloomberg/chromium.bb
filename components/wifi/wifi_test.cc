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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

#if defined(OS_MACOSX)
  // Without this there will be a mem leak on osx.
  base::mac::ScopedNSAutoreleasePool scoped_pool_;
#endif

  // Need AtExitManager to support AsWeakPtr (in NetLog).
  base::AtExitManager exit_manager_;

  Result result_;
};

WiFiTest::Result WiFiTest::Main(int argc, const char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
    VLOG(0) <<  "Usage: " << argv[0] <<
                " [--list]"
                " [--get_properties]"
                " [--connect]"
                " [--disconnect]"
                " [--network_guid=<network_guid>]"
                " [--frequency=0|2400|5000]"
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

  if (parsed_command_line.GetArgs().size() == 1) {
#if defined(OS_WIN)
    network_guid = WideToASCII(parsed_command_line.GetArgs()[0]);
#else
    network_guid = parsed_command_line.GetArgs()[0];
#endif
  }

#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch("debug"))
    MessageBoxA(NULL, __FUNCTION__, "Debug Me!", MB_OK);
#endif

#if defined(OS_WIN)
  scoped_ptr<WiFiService> wifi_service(WiFiService::Create());
#else
  scoped_ptr<WiFiService> wifi_service(WiFiService::CreateForTest());
#endif

  wifi_service->Initialize(NULL);

  if (parsed_command_line.HasSwitch("list")) {
    ListValue network_list;
    wifi_service->GetVisibleNetworks(std::string(), &network_list);
    VLOG(0) << network_list;
    return true;
  }

  if (parsed_command_line.HasSwitch("get_properties")) {
    if (network_guid.length() > 0) {
      DictionaryValue properties;
      std::string error;
      wifi_service->GetProperties(network_guid, &properties, &error);
      VLOG(0) << error << ":\n" << properties;
      return true;
    }
  }

  // Optional properties (frequency, password) to use for connect.
  scoped_ptr<DictionaryValue> connect_properties(new DictionaryValue());

  if (parsed_command_line.HasSwitch("frequency")) {
    int value = 0;
    if (!network_guid.empty() &&
        !frequency.empty() &&
        base::StringToInt(frequency, &value)) {
      connect_properties->SetInteger("WiFi.Frequency", value);
      // fall through to connect.
    }
  }

  if (parsed_command_line.HasSwitch("connect")) {
    if (network_guid.length() > 0) {
      std::string error;
      if (!connect_properties->empty()) {
        VLOG(0) << "Using connect properties: " << *connect_properties;
        wifi_service->SetProperties(network_guid,
                                    connect_properties.Pass(),
                                    &error);
      }
      wifi_service->StartConnect(network_guid, &error);
      VLOG(0) << error;
      return true;
    }
  }

  if (parsed_command_line.HasSwitch("disconnect")) {
    if (network_guid.length() > 0) {
      std::string error;
      wifi_service->StartDisconnect(network_guid, &error);
      VLOG(0) << error;
      return true;
    }
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
