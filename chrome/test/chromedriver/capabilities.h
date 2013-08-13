// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
#define CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

class Log;
class Status;

struct Capabilities {
  Capabilities();
  ~Capabilities();

  // Return true if android package is specified.
  bool IsAndroid() const;

  Status Parse(const base::DictionaryValue& desired_caps, Log* log);

  // Whether the lifetime of the started Chrome browser process should be
  // bound to ChromeDriver's process. If true, Chrome will not quit if
  // ChromeDriver dies.
  bool detach;

  std::string android_package;
  std::string android_activity;
  std::string android_process;
  std::string android_device_serial;
  std::string android_args;

  std::string log_path;
  CommandLine command;
  scoped_ptr<base::DictionaryValue> prefs;
  scoped_ptr<base::DictionaryValue> local_state;
  std::vector<std::string> extensions;
  scoped_ptr<base::DictionaryValue> logging_prefs;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CAPABILITIES_H_
