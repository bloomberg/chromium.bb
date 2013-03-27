// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome_finder.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/user_data_dir.h"
#include "chrome/test/chromedriver/chrome/zip.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

ChromeDesktopImpl::ChromeDesktopImpl(
    URLRequestContextGetter* context_getter,
    int port,
    const SyncWebSocketFactory& socket_factory)
    : ChromeImpl(context_getter, port, socket_factory) {}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  base::CloseProcessHandle(process_);
}

Status ChromeDesktopImpl::Launch(const base::FilePath& exe,
                                 const base::ListValue* args,
                                 const base::ListValue* extensions,
                                 const base::DictionaryValue* prefs,
                                 const base::DictionaryValue* local_state) {
  base::FilePath program = exe;
  if (program.empty()) {
    if (!FindChrome(&program))
      return Status(kUnknownError, "cannot find Chrome binary");
  }
  LOG(INFO) << "Using chrome from " << program.value();

  CommandLine command(program);
  command.AppendSwitchASCII("remote-debugging-port",
                            base::IntToString(GetPort()));
  command.AppendSwitch("no-first-run");
  command.AppendSwitch("enable-logging");
  command.AppendSwitchASCII("logging-level", "1");
  command.AppendArg("data:text/html;charset=utf-8,");

  if (args) {
    Status status = internal::ProcessCommandLineArgs(args, &command);
    if (status.IsError())
      return status;
  }

  if (!command.HasSwitch("user-data-dir")) {
    if (!user_data_dir_.CreateUniqueTempDir())
      return Status(kUnknownError, "cannot create temp dir for user data dir");
    command.AppendSwitchPath("user-data-dir", user_data_dir_.path());
    Status status = internal::PrepareUserDataDir(
        user_data_dir_.path(), prefs, local_state);
    if (status.IsError())
      return status;
  }

  if (extensions) {
    if (!extension_dir_.CreateUniqueTempDir())
      return Status(kUnknownError,
                    "cannot create temp dir for unpacking extensions");
    Status status = internal::ProcessExtensions(
        extensions, extension_dir_.path(), &command);
    if (status.IsError())
      return status;
  }

  base::LaunchOptions options;
  if (!base::LaunchProcess(command, options, &process_))
    return Status(kUnknownError, "chrome failed to start");

  Status status = Init();
  if (status.IsError()) {
    Quit();
    return status;
  }
  return Status(kOk);
}

std::string ChromeDesktopImpl::GetOperatingSystemName() {
  return base::SysInfo::OperatingSystemName();
}

Status ChromeDesktopImpl::Quit() {
  if (!base::KillProcess(process_, 0, true)) {
    int exit_code;
    if (base::GetTerminationStatus(process_, &exit_code) ==
        base::TERMINATION_STATUS_STILL_RUNNING)
      return Status(kUnknownError, "cannot kill Chrome");
  }
  return Status(kOk);
}

namespace internal {

Status ProcessCommandLineArgs(const base::ListValue* args,
                              CommandLine* command) {
  for (size_t i = 0; i < args->GetSize(); ++i) {
    std::string arg_string;
    if (!args->GetString(i, &arg_string))
      return Status(kUnknownError, "invalid chrome command line argument");
    size_t separator_index = arg_string.find("=");
    if (separator_index != std::string::npos) {
      CommandLine::StringType arg_string_native;
      if (!args->GetString(i, &arg_string_native))
        return Status(kUnknownError, "invalid chrome command line argument");
      command->AppendSwitchNative(
          arg_string.substr(0, separator_index),
          arg_string_native.substr(separator_index + 1));
    } else {
      command->AppendSwitch(arg_string);
    }
  }
  return Status(kOk);
}

Status ProcessExtensions(const base::ListValue* extensions,
                         const base::FilePath& temp_dir,
                         CommandLine* command) {
  std::vector<base::FilePath::StringType> extension_paths;
  for (size_t i = 0; i < extensions->GetSize(); ++i) {
    std::string extension_base64;
    if (!extensions->GetString(i, &extension_base64)) {
      return Status(kUnknownError,
                    "each extension must be a base64 encoded string");
    }

    // Decodes extension string.
    // Some WebDriver client base64 encoders follow RFC 1521, which require that
    // 'encoded lines be no more than 76 characters long'. Just remove any
    // newlines.
    RemoveChars(extension_base64, "\n", &extension_base64);
    std::string decoded_extension;
    if (!base::Base64Decode(extension_base64, &decoded_extension))
      return Status(kUnknownError, "failed to base64 decode extension");

    // Writes decoded extension into a temporary .crx file.
    base::ScopedTempDir temp_crx_dir;
    if (!temp_crx_dir.CreateUniqueTempDir())
      return Status(kUnknownError,
                    "cannot create temp dir for writing extension CRX file");
    base::FilePath extension_crx = temp_crx_dir.path().AppendASCII("temp.crx");
    int size = static_cast<int>(decoded_extension.length());
    if (file_util::WriteFile(extension_crx, decoded_extension.c_str(), size)
        != size)
      return Status(kUnknownError, "failed to write extension file");

    // Unzips the temporary .crx file.
    base::FilePath extension_dir = temp_dir.AppendASCII(
        base::StringPrintf("extension%" PRIuS, i));
    if (!zip::Unzip(extension_crx, extension_dir))
      return Status(kUnknownError, "failed to unzip the extension CRX file");
    extension_paths.push_back(extension_dir.value());
  }

  // Sets paths of unpacked extensions to the command line.
  if (!extension_paths.empty()) {
    base::FilePath::StringType extension_paths_value = JoinString(
        extension_paths, FILE_PATH_LITERAL(','));
    command->AppendSwitchNative("load-extension", extension_paths_value);
  }

  return Status(kOk);
}

Status WritePrefsFile(
    const std::string& template_string,
    const base::DictionaryValue* custom_prefs,
    const base::FilePath& path) {
  int code;
  std::string error_msg;
  scoped_ptr<base::Value> template_value(base::JSONReader::ReadAndReturnError(
          template_string, 0, &code, &error_msg));
  base::DictionaryValue* prefs;
  if (!template_value || !template_value->GetAsDictionary(&prefs)) {
    return Status(kUnknownError,
                  "cannot parse internal JSON template: " + error_msg);
  }

  if (custom_prefs)
    prefs->MergeDictionary(custom_prefs);

  std::string prefs_str;
  base::JSONWriter::Write(prefs, &prefs_str);
  if (static_cast<int>(prefs_str.length()) != file_util::WriteFile(
          path, prefs_str.c_str(), prefs_str.length())) {
    return Status(kUnknownError, "failed to write prefs file");
  }
  return Status(kOk);
}

Status PrepareUserDataDir(
    const base::FilePath& user_data_dir,
    const base::DictionaryValue* custom_prefs,
    const base::DictionaryValue* custom_local_state) {
  base::FilePath default_dir = user_data_dir.AppendASCII("Default");
  if (!file_util::CreateDirectory(default_dir))
    return Status(kUnknownError, "cannot create default profile directory");

  Status status = WritePrefsFile(
      kPreferences,
      custom_prefs,
      default_dir.AppendASCII("Preferences"));
  if (status.IsError())
    return status;

  status = WritePrefsFile(
      kLocalState,
      custom_local_state,
      user_data_dir.AppendASCII("Local State"));
  if (status.IsError())
    return status;

  // Write empty "First Run" file, otherwise Chrome will wipe the default
  // profile that was written.
  if (file_util::WriteFile(
          user_data_dir.AppendASCII("First Run"), "", 0) != 0) {
    return Status(kUnknownError, "failed to write first run file");
  }
  return Status(kOk);
}

}  // namespace internal
