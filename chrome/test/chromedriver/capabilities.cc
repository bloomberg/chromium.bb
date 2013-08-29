// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

typedef base::Callback<Status(const base::Value&, Capabilities*)> Parser;

Status ParseBoolean(
    bool* to_set,
    const base::Value& option,
    Capabilities* capabilities) {
  if (!option.GetAsBoolean(to_set))
    return Status(kUnknownError, "value must be a boolean");
  return Status(kOk);
}

Status ParseString(std::string* to_set,
                   const base::Value& option,
                   Capabilities* capabilities) {
  std::string str;
  if (!option.GetAsString(&str))
    return Status(kUnknownError, "value must be a string");
  if (str.empty())
    return Status(kUnknownError, "value cannot be empty");
  *to_set = str;
  return Status(kOk);
}

Status IgnoreDeprecatedOption(
    Log* log,
    const char* option_name,
    const base::Value& option,
    Capabilities* capabilities) {
  log->AddEntry(Log::kWarning,
                base::StringPrintf("deprecated chrome option is ignored: '%s'",
                                   option_name));
  return Status(kOk);
}

Status ParseChromeBinary(
    const base::Value& option,
    Capabilities* capabilities) {
  base::FilePath::StringType path_str;
  if (!option.GetAsString(&path_str))
    return Status(kUnknownError, "'binary' must be a string");
  base::FilePath chrome_exe(path_str);
  capabilities->command.SetProgram(chrome_exe);
  return Status(kOk);
}

Status ParseLogPath(const base::Value& option, Capabilities* capabilities) {
  if (!option.GetAsString(&capabilities->log_path))
    return Status(kUnknownError, "'logPath' must be a string");
  return Status(kOk);
}

Status ParseArgs(bool is_android,
                 const base::Value& option,
                 Capabilities* capabilities) {
  const base::ListValue* args_list = NULL;
  if (!option.GetAsList(&args_list))
    return Status(kUnknownError, "'args' must be a list");
  for (size_t i = 0; i < args_list->GetSize(); ++i) {
    std::string arg_string;
    if (!args_list->GetString(i, &arg_string))
      return Status(kUnknownError, "each argument must be a string");
    if (is_android) {
      capabilities->android_args += "--" + arg_string + " ";
    } else {
      size_t separator_index = arg_string.find("=");
      if (separator_index != std::string::npos) {
        CommandLine::StringType arg_string_native;
        if (!args_list->GetString(i, &arg_string_native))
          return Status(kUnknownError, "each argument must be a string");
        capabilities->command.AppendSwitchNative(
            arg_string.substr(0, separator_index),
            arg_string_native.substr(separator_index + 1));
      } else {
        capabilities->command.AppendSwitch(arg_string);
      }
    }
  }
  return Status(kOk);
}

Status ParsePrefs(const base::Value& option, Capabilities* capabilities) {
  const base::DictionaryValue* prefs = NULL;
  if (!option.GetAsDictionary(&prefs))
    return Status(kUnknownError, "'prefs' must be a dictionary");
  capabilities->prefs.reset(prefs->DeepCopy());
  return Status(kOk);
}

Status ParseLocalState(const base::Value& option, Capabilities* capabilities) {
  const base::DictionaryValue* local_state = NULL;
  if (!option.GetAsDictionary(&local_state))
    return Status(kUnknownError, "'localState' must be a dictionary");
  capabilities->local_state.reset(local_state->DeepCopy());
  return Status(kOk);
}

Status ParseExtensions(const base::Value& option, Capabilities* capabilities) {
  const base::ListValue* extensions = NULL;
  if (!option.GetAsList(&extensions))
    return Status(kUnknownError, "'extensions' must be a list");
  for (size_t i = 0; i < extensions->GetSize(); ++i) {
    std::string extension;
    if (!extensions->GetString(i, &extension)) {
      return Status(kUnknownError,
                    "each extension must be a base64 encoded string");
    }
    capabilities->extensions.push_back(extension);
  }
  return Status(kOk);
}

Status ParseProxy(const base::Value& option, Capabilities* capabilities) {
  const base::DictionaryValue* proxy_dict;
  if (!option.GetAsDictionary(&proxy_dict))
    return Status(kUnknownError, "'proxy' must be a dictionary");
  std::string proxy_type;
  if (!proxy_dict->GetString("proxyType", &proxy_type))
    return Status(kUnknownError, "'proxyType' must be a string");
  proxy_type = StringToLowerASCII(proxy_type);
  if (proxy_type == "direct") {
    capabilities->command.AppendSwitch("no-proxy-server");
  } else if (proxy_type == "system") {
    // Chrome default.
  } else if (proxy_type == "pac") {
    CommandLine::StringType proxy_pac_url;
    if (!proxy_dict->GetString("proxyAutoconfigUrl", &proxy_pac_url))
      return Status(kUnknownError, "'proxyAutoconfigUrl' must be a string");
    capabilities->command.AppendSwitchNative("proxy-pac-url", proxy_pac_url);
  } else if (proxy_type == "autodetect") {
    capabilities->command.AppendSwitch("proxy-auto-detect");
  } else if (proxy_type == "manual") {
    const char* proxy_servers_options[][2] = {
        {"ftpProxy", "ftp"}, {"httpProxy", "http"}, {"sslProxy", "https"}};
    const base::Value* option_value = NULL;
    std::string proxy_servers;
    for (size_t i = 0; i < arraysize(proxy_servers_options); ++i) {
      if (!proxy_dict->Get(proxy_servers_options[i][0], &option_value) ||
          option_value->IsType(base::Value::TYPE_NULL)) {
        continue;
      }
      std::string value;
      if (!option_value->GetAsString(&value)) {
        return Status(
            kUnknownError,
            base::StringPrintf("'%s' must be a string",
                               proxy_servers_options[i][0]));
      }
      // Converts into Chrome proxy scheme.
      // Example: "http=localhost:9000;ftp=localhost:8000".
      if (!proxy_servers.empty())
        proxy_servers += ";";
      proxy_servers += base::StringPrintf(
          "%s=%s", proxy_servers_options[i][1], value.c_str());
    }

    std::string proxy_bypass_list;
    if (proxy_dict->Get("noProxy", &option_value) &&
        !option_value->IsType(base::Value::TYPE_NULL)) {
      if (!option_value->GetAsString(&proxy_bypass_list))
        return Status(kUnknownError, "'noProxy' must be a string");
    }

    if (proxy_servers.empty() && proxy_bypass_list.empty()) {
      return Status(kUnknownError,
                    "proxyType is 'manual' but no manual "
                    "proxy capabilities were found");
    }
    if (!proxy_servers.empty())
      capabilities->command.AppendSwitchASCII("proxy-server", proxy_servers);
    if (!proxy_bypass_list.empty()) {
      capabilities->command.AppendSwitchASCII("proxy-bypass-list",
                                              proxy_bypass_list);
    }
  } else {
    return Status(kUnknownError, "unrecognized proxy type:" + proxy_type);
  }
  return Status(kOk);
}

Status ParseExcludeSwitches(const base::Value& option,
                            Capabilities* capabilities) {
  const base::ListValue* switches = NULL;
  if (!option.GetAsList(&switches))
    return Status(kUnknownError, "'excludeSwitches' must be a list");
  for (size_t i = 0; i < switches->GetSize(); ++i) {
    std::string switch_name;
    if (!switches->GetString(i, &switch_name)) {
      return Status(kUnknownError,
                    "each switch to be removed must be a string");
    }
    capabilities->exclude_switches.insert(switch_name);
  }
  return Status(kOk);
}

Status ParseUseExistingBrowser(const base::Value& option,
                               Capabilities* capabilities) {
  std::string server_addr;
  if (!option.GetAsString(&server_addr))
    return Status(kUnknownError, "must be 'host:port'");

  std::vector<std::string> values;
  base::SplitString(server_addr, ':', &values);
  if (values.size() != 2)
    return Status(kUnknownError, "must be 'host:port'");

  // Ignoring host input for now, hardcoding to 127.0.0.1.
  base::StringToInt(values[1], &capabilities->existing_browser_port);
  if (capabilities->existing_browser_port <= 0)
    return Status(kUnknownError, "port must be >= 0");
  return Status(kOk);
}

Status ParseLoggingPrefs(const base::Value& option,
                         Capabilities* capabilities) {
  const base::DictionaryValue* logging_prefs_dict = NULL;
  if (!option.GetAsDictionary(&logging_prefs_dict))
    return Status(kUnknownError, "'loggingPrefs' must be a dictionary");

  // TODO(klm): verify log types.
  // TODO(klm): verify log levels.
  capabilities->logging_prefs.reset(logging_prefs_dict->DeepCopy());
  return Status(kOk);
}

Status ParseChromeOptions(
    Log* log,
    const base::Value& capability,
    Capabilities* capabilities) {
  const base::DictionaryValue* chrome_options = NULL;
  if (!capability.GetAsDictionary(&chrome_options))
    return Status(kUnknownError, "'chromeOptions' must be a dictionary");

  bool is_android = chrome_options->HasKey("androidPackage");
  bool is_existing = chrome_options->HasKey("useExistingBrowser");

  std::map<std::string, Parser> parser_map;
  if (is_android) {
    parser_map["androidActivity"] =
        base::Bind(&ParseString, &capabilities->android_activity);
    parser_map["androidDeviceSerial"] =
        base::Bind(&ParseString, &capabilities->android_device_serial);
    parser_map["androidPackage"] =
        base::Bind(&ParseString, &capabilities->android_package);
    parser_map["androidProcess"] =
        base::Bind(&ParseString, &capabilities->android_process);
    parser_map["args"] = base::Bind(&ParseArgs, true);
  } else if (is_existing) {
    parser_map["useExistingBrowser"] = base::Bind(&ParseUseExistingBrowser);
  } else {
    parser_map["forceDevToolsScreenshot"] = base::Bind(
        &ParseBoolean, &capabilities->force_devtools_screenshot);
    parser_map["args"] = base::Bind(&ParseArgs, false);
    parser_map["binary"] = base::Bind(&ParseChromeBinary);
    parser_map["detach"] = base::Bind(&ParseBoolean, &capabilities->detach);
    parser_map["excludeSwitches"] = base::Bind(&ParseExcludeSwitches);
    parser_map["extensions"] = base::Bind(&ParseExtensions);
    parser_map["loadAsync"] =
        base::Bind(&IgnoreDeprecatedOption, log, "loadAsync");
    parser_map["localState"] = base::Bind(&ParseLocalState);
    parser_map["logPath"] = base::Bind(&ParseLogPath);
    parser_map["prefs"] = base::Bind(&ParsePrefs);
  }

  for (base::DictionaryValue::Iterator it(*chrome_options); !it.IsAtEnd();
       it.Advance()) {
    if (parser_map.find(it.key()) == parser_map.end()) {
      return Status(kUnknownError,
                    "unrecognized chrome option: " + it.key());
    }
    Status status = parser_map[it.key()].Run(it.value(), capabilities);
    if (status.IsError())
      return Status(kUnknownError, "cannot parse " + it.key(), status);
  }
  return Status(kOk);
}

}  // namespace

Capabilities::Capabilities()
    : force_devtools_screenshot(false),
      detach(false),
      existing_browser_port(0),
      command(CommandLine::NO_PROGRAM) {}

Capabilities::~Capabilities() {}

bool Capabilities::IsAndroid() const {
  return !android_package.empty();
}

bool Capabilities::IsExistingBrowser() const {
  return existing_browser_port > 0;
}

Status Capabilities::Parse(
    const base::DictionaryValue& desired_caps,
    Log* log) {
  std::map<std::string, Parser> parser_map;
  parser_map["chromeOptions"] = base::Bind(&ParseChromeOptions, log);
  parser_map["loggingPrefs"] = base::Bind(&ParseLoggingPrefs);
  parser_map["proxy"] = base::Bind(&ParseProxy);
  for (std::map<std::string, Parser>::iterator it = parser_map.begin();
       it != parser_map.end(); ++it) {
    const base::Value* capability = NULL;
    if (desired_caps.Get(it->first, &capability)) {
      Status status = it->second.Run(*capability, this);
      if (status.IsError()) {
        return Status(
            kUnknownError, "cannot parse capability: " + it->first, status);
      }
    }
  }
  return Status(kOk);
}
