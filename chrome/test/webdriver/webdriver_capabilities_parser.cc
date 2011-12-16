// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_capabilities_parser.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
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
  // Parse WebDriver proxy capabilities first.
  const char kProxyKey[] = "proxy";
  Value* proxy_options_value;
  if (dict_->Get(kProxyKey, &proxy_options_value)) {
    const base::DictionaryValue* proxy_options;
    if (!proxy_options_value->GetAsDictionary(&proxy_options)) {
      return CreateBadInputError(
          kProxyKey, Value::TYPE_DICTIONARY, proxy_options_value);
    }
    Error* error = ParseProxyCapabilities(proxy_options);
    if (error)
      return error;
  }

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
      if (!legacy_options)
        return new Error(kBadRequest,
                         "Unrecognized chrome capability: " +  *key_iter);
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

Error* CapabilitiesParser::ParseProxyCapabilities(
    const base::DictionaryValue* options) {
  // Quick check of proxy capabilities.
  std::set<std::string> proxy_options;
  proxy_options.insert("autodetect");
  proxy_options.insert("ftpProxy");
  proxy_options.insert("httpProxy");
  proxy_options.insert("noProxy");
  proxy_options.insert("proxyType");
  proxy_options.insert("proxyAutoconfigUrl");
  proxy_options.insert("sslProxy");

  base::DictionaryValue::key_iterator key_iter = options->begin_keys();
  for (; key_iter != options->end_keys(); ++key_iter) {
    if (proxy_options.find(*key_iter) == proxy_options.end()) {
      return new Error(kBadRequest,
                       "Unrecognized proxy capability: " + *key_iter);
    }
  }

  typedef Error* (CapabilitiesParser::*Parser)(const DictionaryValue*);
  std::map<std::string, Parser> proxy_type_parser_map;
  proxy_type_parser_map["autodetect"] =
      &CapabilitiesParser::ParseProxyAutoDetect;
  proxy_type_parser_map["pac"] =
      &CapabilitiesParser::ParseProxyAutoconfigUrl;
  proxy_type_parser_map["manual"] = &CapabilitiesParser::ParseProxyServers;
  proxy_type_parser_map["direct"] = NULL;
  proxy_type_parser_map["system"] = NULL;

  Value* option;
  if (!options->Get("proxyType", &option))
    return new Error(kBadRequest, "Missing 'proxyType' capability.");

  std::string proxy_type;
  if (!option->GetAsString(&proxy_type))
    return CreateBadInputError("proxyType", Value::TYPE_STRING, option);

  proxy_type = StringToLowerASCII(proxy_type);
  if (proxy_type_parser_map.find(proxy_type) == proxy_type_parser_map.end())
    return new Error(kBadRequest, "Unrecognized 'proxyType': " + proxy_type);

  if (proxy_type == "direct") {
    caps_->command.AppendSwitch(switches::kNoProxyServer);
  } else if (proxy_type == "system") {
    // Chrome default.
  } else {
    Error* error = (this->*proxy_type_parser_map[proxy_type])(options);
    if (error) {
      error->AddDetails("Error occurred while processing 'proxyType': " +
                        proxy_type);
      return error;
    }
  }
  return NULL;
}

Error* CapabilitiesParser::ParseProxyAutoDetect(
    const base::DictionaryValue* options){
  const char kProxyAutoDetectKey[] = "autodetect";
  bool proxy_auto_detect = false;
  if (!options->GetBoolean(kProxyAutoDetectKey, &proxy_auto_detect))
    return CreateBadInputError(kProxyAutoDetectKey,
                               Value::TYPE_BOOLEAN, options);
  if (proxy_auto_detect)
    caps_->command.AppendSwitch(switches::kProxyAutoDetect);
  return NULL;
}

Error* CapabilitiesParser::ParseProxyAutoconfigUrl(
    const base::DictionaryValue* options){
  const char kProxyAutoconfigUrlKey[] = "proxyAutoconfigUrl";
  CommandLine::StringType proxy_pac_url;
  if (!options->GetString(kProxyAutoconfigUrlKey, &proxy_pac_url))
    return CreateBadInputError(kProxyAutoconfigUrlKey,
                               Value::TYPE_STRING, options);
  caps_->command.AppendSwitchNative(switches::kProxyPacUrl, proxy_pac_url);
  return NULL;
}

Error* CapabilitiesParser::ParseProxyServers(
    const base::DictionaryValue* options) {
  const char kNoProxy[] = "noProxy";
  const char kFtpProxy[] = "ftpProxy";
  const char kHttpProxy[] = "httpProxy";
  const char kSslProxy[] = "sslProxy";

  std::set<std::string> proxy_servers_options;
  proxy_servers_options.insert(kFtpProxy);
  proxy_servers_options.insert(kHttpProxy);
  proxy_servers_options.insert(kSslProxy);

  Error* error = NULL;
  Value* option = NULL;
  bool has_manual_settings = false;
  if (options->Get(kNoProxy, &option)) {
    error = ParseNoProxy(option);
    if (error)
      return error;
    has_manual_settings = true;
  }

  std::vector<std::string> proxy_servers;
  std::set<std::string>::const_iterator iter = proxy_servers_options.begin();
  for (; iter != proxy_servers_options.end(); ++iter) {
    if (options->Get(*iter, &option)) {
      std::string value;
      if (!option->GetAsString(&value))
        return CreateBadInputError(*iter, Value::TYPE_STRING, option);
      has_manual_settings = true;
      // Converts into Chrome proxy scheme.
      // Example: "http=localhost:9000;ftp=localhost:8000".
      if (*iter == kFtpProxy)
        value = "ftp=" + value;
      if (*iter == kHttpProxy)
        value = "http=" + value;
      if (*iter == kSslProxy)
        value = "https=" + value;
      proxy_servers.push_back(value);
    }
  }

  if (!has_manual_settings)
    return new Error(kBadRequest, "proxyType is 'manual' but no manual "
                                  "proxy capabilities were found.");

  std::string proxy_server_value = JoinString(proxy_servers, ';');
  caps_->command.AppendSwitchASCII(switches::kProxyServer, proxy_server_value);

  return NULL;
}

Error* CapabilitiesParser::ParseNoProxy(const base::Value* option){
  std::string proxy_bypass_list;
  if (!option->GetAsString(&proxy_bypass_list))
    return CreateBadInputError("noProxy", Value::TYPE_STRING, option);
  if (!proxy_bypass_list.empty())
    caps_->command.AppendSwitchASCII(switches::kProxyBypassList,
                                     proxy_bypass_list);
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
