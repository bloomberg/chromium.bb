// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_startup.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

namespace extensions_startup {

class PackExtensionLogger : public PackExtensionJob::Client {
 public:
  PackExtensionLogger() : process_startup_(false) {}
  virtual void OnPackSuccess(const FilePath& crx_path,
                             const FilePath& output_private_key_path);
  virtual void OnPackFailure(const std::string& error_message);

 private:
  // We need to track if this extension packing job was created on process
  // startup or not so we know if we should Quit() the message loop after
  // packaging the extension.
  bool process_startup_;
  void ShowPackExtensionMessage(const std::wstring& caption,
                                const std::wstring& message);

  DISALLOW_COPY_AND_ASSIGN(PackExtensionLogger);
};

void PackExtensionLogger::OnPackSuccess(
    const FilePath& crx_path,
    const FilePath& output_private_key_path) {
  ShowPackExtensionMessage(L"Extension Packaging Success",
                           PackExtensionJob::StandardSuccessMessage(
                               crx_path, output_private_key_path));
}

void PackExtensionLogger::OnPackFailure(const std::string& error_message) {
  ShowPackExtensionMessage(L"Extension Packaging Error",
                           UTF8ToWide(error_message));
}

void PackExtensionLogger::ShowPackExtensionMessage(
    const std::wstring& caption,
    const std::wstring& message) {
#if defined(OS_WIN)
  win_util::MessageBox(NULL, message, caption, MB_OK | MB_SETFOREGROUND);
#else
  // Just send caption & text to stdout on mac & linux.
  std::string out_text = WideToASCII(caption);
  out_text.append("\n\n");
  out_text.append(WideToASCII(message));
  out_text.append("\n");
  base::StringPrintf("%s", out_text.c_str());
#endif

  // We got the notification and processed it; we don't expect any further tasks
  // to be posted to the current thread, so we should stop blocking and exit.
  // This call to |Quit()| matches the call to |Run()| in
  // |ProcessCmdLineImpl()|.
  MessageLoop::current()->Quit();
}

bool HandlePackExtension(const CommandLine& cmd_line) {
  if (cmd_line.HasSwitch(switches::kPackExtension)) {
    // Input Paths.
    FilePath src_dir = cmd_line.GetSwitchValuePath(
        switches::kPackExtension);
    FilePath private_key_path;
    if (cmd_line.HasSwitch(switches::kPackExtensionKey)) {
      private_key_path = cmd_line.GetSwitchValuePath(
          switches::kPackExtensionKey);
    }

    // Launch a job to perform the packing on the file thread.
    PackExtensionLogger pack_client;
    scoped_refptr<PackExtensionJob> pack_job(
        new PackExtensionJob(&pack_client, src_dir, private_key_path));
    pack_job->Start();

    // The job will post a notification task to the current thread's message
    // loop when it is finished.  We manually run the loop here so that we
    // block and catch the notification.  Otherwise, the process would exit;
    // in particular, this would mean that |pack_client| would be destroyed
    // and we wouldn't be able to report success or failure back to the user.
    // This call to |Run()| is matched by a call to |Quit()| in the
    // |PackExtensionLogger|'s notification handling code.
    MessageLoop::current()->Run();

    return true;
  }

  return false;
}

bool HandleUninstallExtension(const CommandLine& cmd_line, Profile* profile) {
  DCHECK(profile);

  if (cmd_line.HasSwitch(switches::kUninstallExtension)) {
    ExtensionsService* extensions_service = profile->GetExtensionsService();
    if (extensions_service) {
      std::string extension_id = cmd_line.GetSwitchValueASCII(
          switches::kUninstallExtension);
      if (ExtensionsService::UninstallExtensionHelper(extensions_service,
                                                      extension_id)) {
          return true;
      }
    }
  }

  return false;
}

}  // namespace extensions_startup
