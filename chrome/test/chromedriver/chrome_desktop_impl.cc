// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_desktop_impl.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/zip.h"
#include "chrome/test/chromedriver/chrome_finder.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"

ChromeDesktopImpl::ChromeDesktopImpl(
    URLRequestContextGetter* context_getter,
    int port,
    const SyncWebSocketFactory& socket_factory)
    : ChromeImpl(context_getter, port, socket_factory) {}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  base::CloseProcessHandle(process_);
}

Status ChromeDesktopImpl::Launch(const base::FilePath& chrome_exe,
                                 const base::ListValue* chrome_args,
                                 const base::ListValue* chrome_extensions) {
  base::FilePath program = chrome_exe;
  if (program.empty()) {
    if (!FindChrome(&program))
      return Status(kUnknownError, "cannot find Chrome binary");
  }

  CommandLine command(program);
  command.AppendSwitchASCII("remote-debugging-port",
                            base::IntToString(GetPort()));
  command.AppendSwitch("no-first-run");
  command.AppendSwitch("enable-logging");
  command.AppendSwitchASCII("logging-level", "1");
  if (!user_data_dir_.CreateUniqueTempDir())
    return Status(kUnknownError, "cannot create temp dir for user data dir");
  command.AppendSwitchPath("user-data-dir", user_data_dir_.path());
  command.AppendArg("data:text/html;charset=utf-8,");

  if (chrome_args) {
    Status status = internal::ProcessCommandLineArgs(chrome_args, &command);
    if (status.IsError())
      return status;
  }

  if (chrome_extensions) {
    if (!extension_dir_.CreateUniqueTempDir())
      return Status(kUnknownError,
                    "cannot create temp dir for unpacking extensions");
    Status status = internal::ProcessExtensions(
        chrome_extensions, extension_dir_.path(), &command);
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

}  // namespace internal
