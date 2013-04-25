// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

typedef base::Callback<Status(const base::Value&, Capabilities*)> Parser;

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

Status ParseArgs(const base::Value& option, Capabilities* capabilities) {
  const base::ListValue* args_list = NULL;
  if (!option.GetAsList(&args_list))
    return Status(kUnknownError, "'args' must be a list");
  for (size_t i = 0; i < args_list->GetSize(); ++i) {
    std::string arg_string;
    if (!args_list->GetString(i, &arg_string))
      return Status(kUnknownError, "each argument must be a string");
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

Status ParseDesktopChromeOption(
    const base::Value& capability,
    Capabilities* capabilities) {
  const base::DictionaryValue* chrome_options = NULL;
  if (!capability.GetAsDictionary(&chrome_options))
    return Status(kUnknownError, "'chromeOptions' must be a dictionary");

  std::map<std::string, Parser> parser_map;

  parser_map["binary"] = base::Bind(&ParseChromeBinary);
  parser_map["logPath"] = base::Bind(&ParseLogPath);
  parser_map["args"] = base::Bind(&ParseArgs);
  parser_map["prefs"] = base::Bind(&ParsePrefs);
  parser_map["localState"] = base::Bind(&ParseLocalState);
  parser_map["extensions"] = base::Bind(&ParseExtensions);

  for (base::DictionaryValue::Iterator it(*chrome_options); !it.IsAtEnd();
       it.Advance()) {
    if (parser_map.find(it.key()) == parser_map.end()) {
      return Status(kUnknownError,
                    "unrecognized chrome option: " + it.key());
    }
    Status status = parser_map[it.key()].Run(it.value(), capabilities);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status ParseAndroidChromeCapabilities(const base::DictionaryValue& desired_caps,
                                      Capabilities* capabilities) {
  const base::Value* chrome_options = NULL;
  if (desired_caps.Get("chromeOptions", &chrome_options)) {
    const base::DictionaryValue* chrome_options_dict = NULL;
    if (!chrome_options->GetAsDictionary(&chrome_options_dict))
      return Status(kUnknownError, "'chromeOptions' must be a dictionary");

    const base::Value* android_package_value;
    if (chrome_options_dict->Get("androidPackage", &android_package_value)) {
      if (!android_package_value->GetAsString(&capabilities->android_package) ||
          capabilities->android_package.empty()) {
        return Status(kUnknownError,
                      "'androidPackage' must be a non-empty string");
      }
    }
  }
  return Status(kOk);
}

Status ParseLoggingPrefs(const base::DictionaryValue& desired_caps,
                         Capabilities* capabilities) {
  const base::Value* logging_prefs = NULL;
  if (desired_caps.Get("loggingPrefs", &logging_prefs)) {
    const base::DictionaryValue* logging_prefs_dict = NULL;
    if (!logging_prefs->GetAsDictionary(&logging_prefs_dict)) {
      return Status(kUnknownError, "'loggingPrefs' must be a dictionary");
    }
    // TODO(klm): verify log types.
    // TODO(klm): verify log levels.
    capabilities->logging_prefs.reset(logging_prefs_dict->DeepCopy());
  }
  return Status(kOk);
}

}  // namespace

Capabilities::Capabilities() : command(CommandLine::NO_PROGRAM) {}

Capabilities::~Capabilities() {}

bool Capabilities::IsAndroid() const {
  return !android_package.empty();
}

Status Capabilities::Parse(const base::DictionaryValue& desired_caps) {
  Status status = ParseLoggingPrefs(desired_caps, this);
  if (status.IsError())
    return status;
  status = ParseAndroidChromeCapabilities(desired_caps, this);
  if (status.IsError())
    return status;
  if (IsAndroid())
    return Status(kOk);

  std::map<std::string, Parser> parser_map;
  parser_map["proxy"] = base::Bind(&ParseProxy);
  parser_map["chromeOptions"] = base::Bind(&ParseDesktopChromeOption);
  for (std::map<std::string, Parser>::iterator it = parser_map.begin();
       it != parser_map.end(); ++it) {
    const base::Value* capability = NULL;
    if (desired_caps.Get(it->first, &capability)) {
      status = it->second.Run(*capability, this);
      if (status.IsError())
        return status;
    }
  }
  return Status(kOk);
}
