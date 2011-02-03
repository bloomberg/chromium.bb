// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_startup.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_WIN)
#include "ui/base/message_box_win.h"
#endif

ExtensionsStartupUtil::ExtensionsStartupUtil() : pack_job_succeeded_(false) {}

void ExtensionsStartupUtil::OnPackSuccess(
    const FilePath& crx_path,
    const FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  ShowPackExtensionMessage(
      L"Extension Packaging Success",
      UTF16ToWideHack(PackExtensionJob::StandardSuccessMessage(
          crx_path, output_private_key_path)));
}

void ExtensionsStartupUtil::OnPackFailure(const std::string& error_message) {
  ShowPackExtensionMessage(L"Extension Packaging Error",
                           UTF8ToWide(error_message));
}

void ExtensionsStartupUtil::ShowPackExtensionMessage(
    const std::wstring& caption,
    const std::wstring& message) {
#if defined(OS_WIN)
  ui::MessageBox(NULL, message, caption, MB_OK | MB_SETFOREGROUND);
#else
  // Just send caption & text to stdout on mac & linux.
  std::string out_text = WideToASCII(caption);
  out_text.append("\n\n");
  out_text.append(WideToASCII(message));
  out_text.append("\n");
  base::StringPrintf("%s", out_text.c_str());
#endif
}

bool ExtensionsStartupUtil::PackExtension(const CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(switches::kPackExtension))
    return false;

  // Input Paths.
  FilePath src_dir = cmd_line.GetSwitchValuePath(switches::kPackExtension);
  FilePath private_key_path;
  if (cmd_line.HasSwitch(switches::kPackExtensionKey)) {
    private_key_path = cmd_line.GetSwitchValuePath(switches::kPackExtensionKey);
  }

  // Launch a job to perform the packing on the file thread.
  pack_job_ = new PackExtensionJob(this, src_dir, private_key_path);
  pack_job_->set_asynchronous(false);
  pack_job_->Start();

  return pack_job_succeeded_;
}

bool ExtensionsStartupUtil::UninstallExtension(const CommandLine& cmd_line,
                                               Profile* profile) {
  DCHECK(profile);

  if (!cmd_line.HasSwitch(switches::kUninstallExtension))
    return false;

  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service)
    return false;

  std::string extension_id = cmd_line.GetSwitchValueASCII(
      switches::kUninstallExtension);
  if (ExtensionService::UninstallExtensionHelper(extension_service,
                                                 extension_id)) {
    return true;
  }

  return false;
}

ExtensionsStartupUtil::~ExtensionsStartupUtil() {
  if (pack_job_.get())
    pack_job_->ClearClient();
}
