// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_capabilities_parser.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/zip.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_util.h"

using base::Value;

namespace webdriver {

namespace {

Error* CreateBadInputError(const std::string& name,
                           Value::Type type,
                           const Value* option) {
  return new Error(kBadRequest, base::StringPrintf(
      "%s must be of type %s, not %s. Received: %s",
      name.c_str(), GetJsonTypeName(type),
      GetJsonTypeName(option->GetType()),
      JsonStringifyForDisplay(option).c_str()));
}

}  // namespace

Capabilities::Capabilities()
    : command(CommandLine::NO_PROGRAM),
      detach(false),
      load_async(false),
      native_events(false),
      no_website_testing_defaults(false),
      verbose(false) { }

Capabilities::~Capabilities() { }

CapabilitiesParser::CapabilitiesParser(
    const base::DictionaryValue* capabilities_dict,
    const FilePath& root_path,
    Capabilities* capabilities)
    : dict_(capabilities_dict),
      root_(root_path),
      caps_(capabilities) {
}

CapabilitiesParser::~CapabilitiesParser() { }

Error* CapabilitiesParser::Parse() {
  const char kOptionsKey[] = "chromeOptions";
  const base::DictionaryValue* options = dict_;
  bool legacy_options = true;
  Value* options_value;
  if (dict_->Get(kOptionsKey, &options_value)) {
    legacy_options = false;
    if (options_value->IsType(Value::TYPE_DICTIONARY)) {
      options = static_cast<DictionaryValue*>(options_value);
    } else {
      return CreateBadInputError(
          kOptionsKey, Value::TYPE_DICTIONARY, options_value);
    }
  }

  typedef Error* (CapabilitiesParser::*Parser)(const Value*);
  std::map<std::string, Parser> parser_map;
  if (legacy_options) {
    parser_map["chrome.binary"] = &CapabilitiesParser::ParseBinary;
    parser_map["chrome.channel"] = &CapabilitiesParser::ParseChannel;
    parser_map["chrome.detach"] = &CapabilitiesParser::ParseDetach;
    parser_map["chrome.extensions"] = &CapabilitiesParser::ParseExtensions;
    parser_map["chrome.loadAsync"] = &CapabilitiesParser::ParseLoadAsync;
    parser_map["chrome.nativeEvents"] = &CapabilitiesParser::ParseNativeEvents;
    parser_map["chrome.profile"] = &CapabilitiesParser::ParseProfile;
    parser_map["chrome.switches"] = &CapabilitiesParser::ParseArgs;
    parser_map["chrome.noWebsiteTestingDefaults"] =
        &CapabilitiesParser::ParseNoWebsiteTestingDefaults;
    parser_map["chrome.verbose"] = &CapabilitiesParser::ParseVerbose;
  } else {
    parser_map["args"] = &CapabilitiesParser::ParseArgs;
    parser_map["binary"] = &CapabilitiesParser::ParseBinary;
    parser_map["channel"] = &CapabilitiesParser::ParseChannel;
    parser_map["detach"] = &CapabilitiesParser::ParseDetach;
    parser_map["extensions"] = &CapabilitiesParser::ParseExtensions;
    parser_map["loadAsync"] = &CapabilitiesParser::ParseLoadAsync;
    parser_map["nativeEvents"] = &CapabilitiesParser::ParseNativeEvents;
    parser_map["profile"] = &CapabilitiesParser::ParseProfile;
    parser_map["noWebsiteTestingDefaults"] =
        &CapabilitiesParser::ParseNoWebsiteTestingDefaults;
    parser_map["verbose"] = &CapabilitiesParser::ParseVerbose;
  }

  base::DictionaryValue::key_iterator key_iter = options->begin_keys();
  for (; key_iter != options->end_keys(); ++key_iter) {
    if (parser_map.find(*key_iter) == parser_map.end()) {
      LOG(WARNING) << "Ignoring unrecognized capability: " << *key_iter;
      continue;
    }
    Value* option = NULL;
    options->GetWithoutPathExpansion(*key_iter, &option);
    Error* error = (this->*parser_map[*key_iter])(option);
    if (error) {
      error->AddDetails(base::StringPrintf(
          "Error occurred while processing capability '%s'",
          (*key_iter).c_str()));
      return error;
    }
  }
  return NULL;
}

Error* CapabilitiesParser::ParseArgs(const Value* option) {
  const base::ListValue* args;
  if (!option->GetAsList(&args))
    return CreateBadInputError("arguments", Value::TYPE_LIST, option);
  for (size_t i = 0; i < args->GetSize(); ++i) {
    std::string arg_string;
    if (!args->GetString(i, &arg_string))
      return CreateBadInputError("argument", Value::TYPE_STRING, option);
    size_t separator_index = arg_string.find("=");
    if (separator_index != std::string::npos) {
      CommandLine::StringType arg_string_native;
      if (!args->GetString(i, &arg_string_native))
        return CreateBadInputError("argument", Value::TYPE_STRING, option);
      caps_->command.AppendSwitchNative(
          arg_string.substr(0, separator_index),
          arg_string_native.substr(separator_index + 1));
    } else {
      caps_->command.AppendSwitch(arg_string);
    }
  }
  return NULL;
}

Error* CapabilitiesParser::ParseBinary(const Value* option) {
  FilePath::StringType path;
  if (!option->GetAsString(&path)) {
    return CreateBadInputError("binary path", Value::TYPE_STRING, option);
  }
  caps_->command.SetProgram(FilePath(path));
  return NULL;
}

Error* CapabilitiesParser::ParseChannel(const Value* option) {
  if (!option->GetAsString(&caps_->channel))
    return CreateBadInputError("channel", Value::TYPE_STRING, option);
  return NULL;
}

Error* CapabilitiesParser::ParseDetach(const Value* option) {
  if (!option->GetAsBoolean(&caps_->detach))
    return CreateBadInputError("detach", Value::TYPE_BOOLEAN, option);
  return NULL;
}

Error* CapabilitiesParser::ParseExtensions(const Value* option) {
  const base::ListValue* extensions;
  if (!option->GetAsList(&extensions))
    return CreateBadInputError("extensions", Value::TYPE_LIST, option);
  for (size_t i = 0; i < extensions->GetSize(); ++i) {
    std::string extension_base64;
    if (!extensions->GetString(i, &extension_base64)) {
      return new Error(kBadRequest,
                       "Each extension must be a base64 encoded string");
    }
    FilePath extension = root_.AppendASCII(
        base::StringPrintf("extension%" PRIuS ".crx", i));
    std::string error_msg;
    if (!DecodeAndWriteFile(extension, extension_base64, false /* unzip */,
                            &error_msg)) {
      return new Error(
          kUnknownError,
          "Error occurred while parsing extension: " + error_msg);
    }
    caps_->extensions.push_back(extension);
  }
  return NULL;
}

Error* CapabilitiesParser::ParseLoadAsync(const Value* option) {
  if (!option->GetAsBoolean(&caps_->load_async))
    return CreateBadInputError("loadAsync", Value::TYPE_BOOLEAN, option);
  return NULL;
}

Error* CapabilitiesParser::ParseNativeEvents(const Value* option) {
  if (!option->GetAsBoolean(&caps_->native_events))
    return CreateBadInputError("nativeEvents", Value::TYPE_BOOLEAN, option);
  return NULL;
}

Error* CapabilitiesParser::ParseProfile(const Value* option) {
  std::string profile_base64;
  if (!option->GetAsString(&profile_base64))
    return CreateBadInputError("profile", Value::TYPE_STRING, option);
  std::string error_msg;
  caps_->profile = root_.AppendASCII("profile");
  if (!DecodeAndWriteFile(caps_->profile, profile_base64, true /* unzip */,
                          &error_msg))
    return new Error(kUnknownError, "unable to unpack profile: " + error_msg);
  return NULL;
}

Error* CapabilitiesParser::ParseNoWebsiteTestingDefaults(const Value* option) {
  if (!option->GetAsBoolean(&caps_->no_website_testing_defaults))
    return CreateBadInputError("noWebsiteTestingDefaults",
                               Value::TYPE_BOOLEAN, option);
  return NULL;
}

Error* CapabilitiesParser::ParseVerbose(const Value* option) {
  if (!option->GetAsBoolean(&caps_->verbose))
    return CreateBadInputError("verbose", Value::TYPE_BOOLEAN, option);
  return NULL;
}

bool CapabilitiesParser::DecodeAndWriteFile(
    const FilePath& path,
    const std::string& base64,
    bool unzip,
    std::string* error_msg) {
  std::string data;
  if (!base::Base64Decode(base64, &data)) {
    *error_msg = "Could not decode base64 data";
    return false;
  }
  if (unzip) {
    FilePath temp_file(root_.AppendASCII(GenerateRandomID()));
    if (!file_util::WriteFile(temp_file, data.c_str(), data.length())) {
      *error_msg = "Could not write file";
      return false;
    }
    if (!zip::Unzip(temp_file, path)) {
      *error_msg = "Could not unzip archive";
      return false;
    }
  } else {
    if (!file_util::WriteFile(path, data.c_str(), data.length())) {
      *error_msg = "Could not write file";
      return false;
    }
  }
  return true;
}

}  // namespace webdriver
