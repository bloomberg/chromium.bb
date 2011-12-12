// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"

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
  std::vector<FilePath> extensions;

  // Whether Chrome should not block when loading.
  bool load_async;

  // Whether Chrome should simulate input events using OS APIs instead of
  // WebKit APIs.
  bool native_events;

  // By default, ChromeDriver configures Chrome in such a way as convenient
  // for website testing. E.g., it configures Chrome so that sites are allowed
  // to use the geolocation API without requesting the user's consent.
  // If this is set to true, ChromeDriver will not modify Chrome's default
  // behavior.
  bool no_website_testing_defaults;

  // Path to a custom profile to use.
  FilePath profile;

  // Whether ChromeDriver should log verbosely.
  bool verbose;
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
                     const FilePath& root_path,
                     Capabilities* capabilities);
  ~CapabilitiesParser();

  // Parses the capabilities. May return an error.
  Error* Parse();

 private:
  Error* ParseArgs(const base::Value* option);
  Error* ParseBinary(const base::Value* option);
  Error* ParseChannel(const base::Value* option);
  Error* ParseDetach(const base::Value* option);
  Error* ParseExtensions(const base::Value* option);
  Error* ParseLoadAsync(const base::Value* option);
  Error* ParseNativeEvents(const base::Value* option);
  Error* ParseProfile(const base::Value* option);
  Error* ParseNoWebsiteTestingDefaults(const base::Value* option);
  Error* ParseVerbose(const base::Value* option);
  // Decodes the given base64-encoded string, optionally unzips it, and
  // writes the result to |path|.
  // On error, false will be returned and |error_msg| will be set.
  bool DecodeAndWriteFile(const FilePath& path,
                          const std::string& base64,
                          bool unzip,
                          std::string* error_msg);

  // The capabilities dictionary to parse.
  const base::DictionaryValue* dict_;

  // The root directory under which to write all files.
  const FilePath root_;

  // A pointer to the capabilities to modify while parsing.
  Capabilities* caps_;

  DISALLOW_COPY_AND_ASSIGN(CapabilitiesParser);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_CAPABILITIES_PARSER_H_
