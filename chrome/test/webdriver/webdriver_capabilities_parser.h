// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/webdriver/webdriver_logging.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace webdriver {

class Error;

// Contains all the capabilities that a user may request when starting a
// new session.
struct Capabilities {
  Capabilities();
  ~Capabilities();

  // Command line to use for starting Chrome.
  CommandLine command;

  // Channel ID to use for connecting to an already running Chrome.
  std::string channel;

  // Whether the lifetime of the started Chrome browser process should be
  // bound to ChromeDriver's process. If true, Chrome will not quit if
  // ChromeDriver dies.
  bool detach;

  // List of paths for extensions to install on startup.
  std::vector<base::FilePath> extensions;

  // Whether Chrome should not block when loading.
  bool load_async;

  // Local state preferences to apply after Chrome starts but during session
  // initialization. These preferences apply to all profiles in the user
  // data directory that Chrome is running in.
  scoped_ptr<base::DictionaryValue> local_state;

  // The minimum level to log for each log type.
  LogLevel log_levels[LogType::kNum];

  // By default, ChromeDriver configures Chrome in such a way as convenient
  // for website testing. E.g., it configures Chrome so that sites are allowed
  // to use the geolocation API without requesting the user's consent.
  // If this is set to true, ChromeDriver will not modify Chrome's default
  // behavior.
  bool no_website_testing_defaults;

  // Set of switches which should be removed from default list when launching
  // Chrome.
  std::set<std::string> exclude_switches;

  // Profile-level preferences to apply after Chrome starts but during session
  // initialization.
  scoped_ptr<base::DictionaryValue> prefs;

  // Path to a custom profile to use.
  base::FilePath profile;
};

// Parses the given capabilities dictionary to produce a |Capabilities|
// instance.
// See webdriver's desired capabilities for more info.
class CapabilitiesParser {
 public:
  // Create a new parser. |capabilities_dict| is the dictionary for all
  // of the user's desired capabilities. |root_path| is the root directory
  // to use for writing any necessary files to disk. This function will not
  // create it or delete it. All files written to disk will be placed in
  // this directory.
  CapabilitiesParser(const base::DictionaryValue* capabilities_dict,
                     const base::FilePath& root_path,
                     const Logger& logger,
                     Capabilities* capabilities);
  ~CapabilitiesParser();

  // Parses the capabilities. May return an error.
  Error* Parse();

 private:
  Error* ParseArgs(const base::Value* option);
  Error* ParseBinary(const base::Value* option);
  Error* ParseChannel(const base::Value* option);
  Error* ParseDetach(const base::Value* option);
  Error* ParseExcludeSwitches(const base::Value* options);
  Error* ParseExtensions(const base::Value* option);
  Error* ParseLoadAsync(const base::Value* option);
  Error* ParseLocalState(const base::Value* option);
  Error* ParseLoggingPrefs(const base::Value* option);
  Error* ParseNativeEvents(const base::Value* option);
  Error* ParseNoProxy(const base::Value* option);
  Error* ParseNoWebsiteTestingDefaults(const base::Value* option);
  Error* ParsePrefs(const base::Value* option);
  Error* ParseProfile(const base::Value* option);
  Error* ParseProxy(const base::Value* option);
  Error* ParseProxyAutoDetect(const base::DictionaryValue* options);
  Error* ParseProxyAutoconfigUrl(const base::DictionaryValue* options);
  Error* ParseProxyServers(const base::DictionaryValue* options);


  // The capabilities dictionary to parse.
  const base::DictionaryValue* dict_;

  // The root directory under which to write all files.
  const base::FilePath root_;

  // Reference to the logger to use.
  const Logger& logger_;

  // A pointer to the capabilities to modify while parsing.
  Capabilities* caps_;

  DISALLOW_COPY_AND_ASSIGN(CapabilitiesParser);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_
