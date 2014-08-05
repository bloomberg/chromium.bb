// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
#define CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/net/net_util.h"

namespace base {
class CommandLine;
class DictionaryValue;
}

class Status;

class Switches {
 public:
  typedef base::FilePath::StringType NativeString;
  Switches();
  ~Switches();

  void SetSwitch(const std::string& name);
  void SetSwitch(const std::string& name, const std::string& value);
  void SetSwitch(const std::string& name, const base::string16& value);
  void SetSwitch(const std::string& name, const base::FilePath& value);

  // In case of same key, |switches| will override.
  void SetFromSwitches(const Switches& switches);

  // Sets a switch from the capabilities, of the form [--]name[=value].
  void SetUnparsedSwitch(const std::string& unparsed_switch);

  void RemoveSwitch(const std::string& name);

  bool HasSwitch(const std::string& name) const;
  std::string GetSwitchValue(const std::string& name) const;
  NativeString GetSwitchValueNative(const std::string& name) const;

  size_t GetSize() const;

  void AppendToCommandLine(base::CommandLine* command) const;
  std::string ToString() const;

 private:
  typedef std::map<std::string, NativeString> SwitchMap;
  SwitchMap switch_map_;
};

typedef std::map<std::string, Log::Level> LoggingPrefs;

struct PerfLoggingPrefs {
  PerfLoggingPrefs();
  ~PerfLoggingPrefs();

  // We must distinguish between a log domain being set by default and being
  // explicitly set. Otherwise, |PerformanceLogger| could only handle 3 of 4
  // possible combinations (tracing enabled/disabled + Timeline on/off).
  enum InspectorDomainStatus {
    kDefaultEnabled,
    kDefaultDisabled,
    kExplicitlyEnabled,
    kExplicitlyDisabled
  };

  InspectorDomainStatus network;
  InspectorDomainStatus page;
  InspectorDomainStatus timeline;

  std::string trace_categories;  // Non-empty string enables tracing.
};

struct Capabilities {
  Capabilities();
  ~Capabilities();

  // Return true if remote host:port session is to be used.
  bool IsRemoteBrowser() const;

  // Return true if android package is specified.
  bool IsAndroid() const;

  Status Parse(const base::DictionaryValue& desired_caps);

  std::string android_activity;

  std::string android_device_serial;

  std::string android_package;

  std::string android_process;

  bool android_use_running_app;

  base::FilePath binary;

  // If provided, the remote debugging address to connect to.
  NetAddress debugger_address;

  // Whether the lifetime of the started Chrome browser process should be
  // bound to ChromeDriver's process. If true, Chrome will not quit if
  // ChromeDriver dies.
  bool detach;

  // Device metrics for use in Device Emulation.
  scoped_ptr<DeviceMetrics> device_metrics;

  // Set of switches which should be removed from default list when launching
  // Chrome.
  std::set<std::string> exclude_switches;

  std::vector<std::string> extensions;

  // True if should always use DevTools for taking screenshots.
  // This is experimental and may be removed at a later point.
  bool force_devtools_screenshot;

  scoped_ptr<base::DictionaryValue> local_state;

  std::string log_path;

  LoggingPrefs logging_prefs;

  // If set, enable minidump for chrome crashes and save to this directory.
  std::string minidump_path;

  PerfLoggingPrefs perf_logging_prefs;

  scoped_ptr<base::DictionaryValue> prefs;

  Switches switches;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
