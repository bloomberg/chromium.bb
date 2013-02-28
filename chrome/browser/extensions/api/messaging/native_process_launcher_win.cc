// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "crypto/random.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"

namespace extensions {

const wchar_t kNativeMessagingRegistryKey[] =
    L"SOFTWARE\\Google\\Chrome\\NativeMessagingHosts";

// static
scoped_ptr<NativeMessagingHostManifest>
NativeProcessLauncher::FindAndLoadManifest(
    const std::string& native_host_name,
    std::string* error_message) {
  base::win::RegKey key;

  string16 manifest_path;
  string16 native_host_name_wide = UTF8ToUTF16(native_host_name);

  bool found = false;

  // First check 32-bit registry and then try 64-bit.
  if (key.Open(HKEY_LOCAL_MACHINE, kNativeMessagingRegistryKey,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    if (key.ReadValue(native_host_name_wide.c_str(), &manifest_path) ==
        ERROR_SUCCESS) {
      found = true;
    }
  }

  if (!found && key.Open(HKEY_LOCAL_MACHINE, kNativeMessagingRegistryKey,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY) == ERROR_SUCCESS) {
    if (key.ReadValue(native_host_name_wide.c_str(), &manifest_path) ==
        ERROR_SUCCESS) {
      found = true;
    }
  }

  if (!found) {
    *error_message = "Native messaging host " + native_host_name +
        " is not registered";
    return scoped_ptr<NativeMessagingHostManifest>();
  }

  return NativeMessagingHostManifest::Load(
      base::FilePath(manifest_path), error_message);
}

// static
bool NativeProcessLauncher::LaunchNativeProcess(
    const base::FilePath& path,
    base::PlatformFile* read_file,
    base::PlatformFile* write_file) {
  // Timeout for the IO pipes.
  const DWORD kTimeoutMs = 5000;

  // Windows will use default buffer size when 0 is passed to
  // CreateNamedPipeW().
  const DWORD kBufferSize = 0;

  if (!path.IsAbsolute()) {
    LOG(ERROR) << "Native Messaging host path must be absolute.";
    return false;
  }

  uint64 pipe_name_token;
  crypto::RandBytes(&pipe_name_token, sizeof(pipe_name_token));
  string16 out_pipe_name = base::StringPrintf(
      L"\\\\.\\pipe\\chrome.nativeMessaging.out.%llx", pipe_name_token);
  string16 in_pipe_name = base::StringPrintf(
      L"\\\\.\\pipe\\chrome.nativeMessaging.in.%llx", pipe_name_token);

  // Create the pipes to read and write from.
  base::win::ScopedHandle stdout_pipe(
      CreateNamedPipeW(out_pipe_name.c_str(),
                       PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED |
                           FILE_FLAG_FIRST_PIPE_INSTANCE,
                       PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize,
                       kTimeoutMs, NULL));
  if (!stdout_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << out_pipe_name;
    return false;
  }

  base::win::ScopedHandle stdin_pipe(
      CreateNamedPipeW(in_pipe_name.c_str(),
                       PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                           FILE_FLAG_FIRST_PIPE_INSTANCE,
                       PIPE_TYPE_BYTE, 1, kBufferSize, kBufferSize,
                       kTimeoutMs, NULL));
  if (!stdin_pipe.IsValid()) {
    LOG(ERROR) << "Failed to create pipe " << in_pipe_name;
    return false;
  }

  DWORD comspec_length = ::GetEnvironmentVariable(L"COMSPEC", NULL, 0);
  if (comspec_length == 0) {
    LOG(ERROR) << "COMSPEC is not set";
    return false;
  }
  scoped_ptr<wchar_t[]> comspec(new wchar_t[comspec_length]);
  ::GetEnvironmentVariable(L"COMSPEC", comspec.get(), comspec_length);

  // 'start' command has a moronic syntax: if first argument is quoted then it
  // interprets it as a command title. Host path must always be in quotes, so
  // we always need to specify the title as the first argument.
  string16 command = base::StringPrintf(
      L"%ls /c start \"Chrome Native Messaging Host\" /b "
      L"\"%ls\" < %ls > %ls",
      comspec.get(), path.value().c_str(),
      in_pipe_name.c_str(), out_pipe_name.c_str());

  base::LaunchOptions options;
  options.start_hidden = true;
  base::ProcessHandle cmd_handle;
  if (!base::LaunchProcess(command.c_str(), options, &cmd_handle)) {
    LOG(ERROR) << "Error launching process " << path.MaybeAsASCII();
    return false;
  }

  bool stdout_connected = ConnectNamedPipe(stdout_pipe.Get(), NULL) ?
      TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
  bool stdin_connected = ConnectNamedPipe(stdin_pipe.Get(), NULL) ?
      TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
  if (!stdout_connected || !stdin_connected) {
    base::KillProcess(cmd_handle, 0, false);
    base::CloseProcessHandle(cmd_handle);
    LOG(ERROR) << "Failed to connect IO pipes when starting "
               << path.MaybeAsASCII();
    return false;
  }

  // Check that cmd.exe has completed with 0 exit code to make sure it was
  // able to connect IO pipes.
  int error_code;
  if (!base::WaitForExitCodeWithTimeout(
          cmd_handle, &error_code,
          base::TimeDelta::FromMilliseconds(kTimeoutMs)) ||
      error_code != 0) {
    LOG(ERROR) << "cmd.exe did not exit cleanly";
    base::KillProcess(cmd_handle, 0, false);
    base::CloseProcessHandle(cmd_handle);
    return false;
  }

  base::CloseProcessHandle(cmd_handle);

  *read_file = stdout_pipe.Take();
  *write_file = stdin_pipe.Take();

  return true;
}

}  // namespace extensions
