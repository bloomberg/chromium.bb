// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/create_session.h"

#include <sstream>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/zip.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"

namespace webdriver {

namespace {

bool WriteBase64DataToFile(const FilePath& filename,
                           const std::string& base64data,
                           std::string* error_msg) {
  std::string data;
  if (!base::Base64Decode(base64data, &data)) {
    *error_msg = "Invalid base64 encoded data.";
    return false;
  }
  if (!file_util::WriteFile(filename, data.c_str(), data.length())) {
    *error_msg = "Could not write data to file.";
    return false;
  }
  return true;
}

Error* GetBooleanCapability(
    const base::DictionaryValue* dict, const std::string& key, bool* option) {
  Value* value = NULL;
  if (dict->GetWithoutPathExpansion(key, &value)) {
    if (!value->GetAsBoolean(option)) {
      return new Error(kUnknownError, key + " must be a boolean");
    }
  }
  return NULL;
}

}  // namespace

CreateSession::CreateSession(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : Command(path_segments, parameters) {}

CreateSession::~CreateSession() {}

bool CreateSession::DoesPost() { return true; }

void CreateSession::ExecutePost(Response* const response) {
  DictionaryValue *capabilities = NULL;
  if (!GetDictionaryParameter("desiredCapabilities", &capabilities)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'desiredCapabilities'"));
    return;
  }

  Automation::BrowserOptions browser_options;
  FilePath::StringType path;
  if (capabilities->GetStringWithoutPathExpansion("chrome.binary", &path))
    browser_options.command = CommandLine(FilePath(path));

  ListValue* switches = NULL;
  const char* kCustomSwitchesKey = "chrome.switches";
  if (capabilities->GetListWithoutPathExpansion(kCustomSwitchesKey,
                                                &switches)) {
    for (size_t i = 0; i < switches->GetSize(); ++i) {
      std::string switch_string;
      if (!switches->GetString(i, &switch_string)) {
        response->SetError(new Error(
            kBadRequest, "Custom switch is not a string"));
        return;
      }
      size_t separator_index = switch_string.find("=");
      if (separator_index != std::string::npos) {
        CommandLine::StringType switch_string_native;
        if (!switches->GetString(i, &switch_string_native)) {
          response->SetError(new Error(
              kBadRequest, "Custom switch is not a string"));
          return;
        }
        browser_options.command.AppendSwitchNative(
            switch_string.substr(0, separator_index),
            switch_string_native.substr(separator_index + 1));
      } else {
        browser_options.command.AppendSwitch(switch_string);
      }
    }
  } else if (capabilities->HasKey(kCustomSwitchesKey)) {
    response->SetError(new Error(
        kBadRequest, "Custom switches must be a list"));
    return;
  }

  Value* verbose_value;
  if (capabilities->GetWithoutPathExpansion("chrome.verbose", &verbose_value)) {
    bool verbose = false;
    if (verbose_value->GetAsBoolean(&verbose)) {
      // Since logging is shared among sessions, if any session requests verbose
      // logging, verbose logging will be enabled for all sessions. It is not
      // possible to turn it off.
      if (verbose)
        logging::SetMinLogLevel(logging::LOG_INFO);
    } else {
      response->SetError(new Error(
          kBadRequest, "verbose must be a boolean true or false"));
      return;
    }
  }

  capabilities->GetStringWithoutPathExpansion(
      "chrome.channel", &browser_options.channel_id);

  ScopedTempDir temp_profile_dir;
  std::string base64_profile;
  if (capabilities->GetStringWithoutPathExpansion("chrome.profile",
                                                  &base64_profile)) {
    if (!temp_profile_dir.CreateUniqueTempDir()) {
      response->SetError(new Error(
          kBadRequest, "Could not create temporary profile directory."));
      return;
    }
    FilePath temp_profile_zip(
        temp_profile_dir.path().AppendASCII("profile.zip"));
    std::string message;
    if (!WriteBase64DataToFile(temp_profile_zip, base64_profile, &message)) {
      response->SetError(new Error(kBadRequest, message));
      return;
    }

    browser_options.user_data_dir =
        temp_profile_dir.path().AppendASCII("user_data_dir");
    if (!Unzip(temp_profile_zip, browser_options.user_data_dir)) {
      response->SetError(new Error(
          kBadRequest, "Could not unarchive provided user profile"));
      return;
    }
  }

  const char* kExtensions = "chrome.extensions";
  ScopedTempDir extensions_dir;
  ListValue* extensions_list = NULL;
  std::vector<FilePath> extensions;
  if (capabilities->GetListWithoutPathExpansion(kExtensions,
                                                &extensions_list)) {
    if (!extensions_dir.CreateUniqueTempDir()) {
      response->SetError(new Error(
          kBadRequest, "Could create temporary extensions directory."));
      return;
    }
    for (size_t i = 0; i < extensions_list->GetSize(); ++i) {
      std::string base64_extension;
      if (!extensions_list->GetString(i, &base64_extension)) {
        response->SetError(new Error(
            kBadRequest, "Extension must be a base64 encoded string."));
        return;
      }
      FilePath extension_file(
          extensions_dir.path().AppendASCII("extension" +
                                            base::IntToString(i) + ".crx"));
      std::string message;
      if (!WriteBase64DataToFile(extension_file, base64_extension, &message)) {
        response->SetError(new Error(kBadRequest, message));
        return;
      }
      extensions.push_back(extension_file);
    }
  } else if (capabilities->HasKey(kExtensions)) {
    response->SetError(new Error(
        kBadRequest, "Extensions must be a list of base64 encoded strings"));
    return;
  }

  Session::Options session_options;
  Error* error = NULL;
  error = GetBooleanCapability(capabilities, "chrome.nativeEvents",
                               &session_options.use_native_events);
  if (!error) {
    error = GetBooleanCapability(capabilities, "chrome.loadAsync",
                                 &session_options.load_async);
  }
  if (error) {
    response->SetError(error);
    return;
  }

  // Session manages its own liftime, so do not call delete.
  Session* session = new Session(session_options);
  error = session->Init(browser_options);
  if (error) {
    response->SetError(error);
    return;
  }

  // Install extensions.
  for (size_t i = 0; i < extensions.size(); ++i) {
    Error* error = session->InstallExtension(extensions[i]);
    if (error) {
      response->SetError(error);
      return;
    }
  }

  LOG(INFO) << "Created session " << session->id();
  // Redirect to a relative URI. Although prohibited by the HTTP standard,
  // this is what the IEDriver does. Finding the actual IP address is
  // difficult, and returning the hostname causes perf problems with the python
  // bindings on Windows.
  std::ostringstream stream;
  stream << SessionManager::GetInstance()->url_base() << "/session/"
         << session->id();
  response->SetStatus(kSeeOther);
  response->SetValue(Value::CreateStringValue(stream.str()));
}

}  // namespace webdriver
