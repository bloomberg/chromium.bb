// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_breakpad_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"

#if defined(OS_ANDROID)
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

ShellBreakpadClient::ShellBreakpadClient() {}
ShellBreakpadClient::~ShellBreakpadClient() {}

#if defined(OS_WIN)
void ShellBreakpadClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  *product_name = base::ASCIIToUTF16("content_shell");
  *version = base::ASCIIToUTF16(CONTENT_SHELL_VERSION);
  *special_build = base::string16();
  *channel_name = base::string16();
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
void ShellBreakpadClient::GetProductNameAndVersion(std::string* product_name,
                                                   std::string* version) {
  *product_name = "content_shell";
  *version = CONTENT_SHELL_VERSION;
}

base::FilePath ShellBreakpadClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

bool ShellBreakpadClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kCrashDumpsDir))
    return false;
  *crash_dir = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kCrashDumpsDir);
  return true;
}

#if defined(OS_ANDROID)
int ShellBreakpadClient::GetAndroidMinidumpDescriptor() {
  return kAndroidMinidumpDescriptor;
}
#endif

bool ShellBreakpadClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPluginProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace content
