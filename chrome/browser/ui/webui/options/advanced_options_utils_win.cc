// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/advanced_options_utils.h"

#include <windows.h>
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#include <shellapi.h>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/win/hwnd_util.h"

using content::BrowserThread;
using content::WebContents;

namespace options {

// Callback that opens the Internet Options control panel dialog with the
// Connections tab selected.
void OpenConnectionDialogCallback() {
  // Using rundll32 seems better than LaunchConnectionDialog which causes a
  // new dialog to be made for each call.  rundll32 uses the same global
  // dialog and it seems to share with the shortcut in control panel.
  base::FilePath rundll32;
  PathService::Get(base::DIR_SYSTEM, &rundll32);
  rundll32 = rundll32.AppendASCII("rundll32.exe");

  base::FilePath shell32dll;
  PathService::Get(base::DIR_SYSTEM, &shell32dll);
  shell32dll = shell32dll.AppendASCII("shell32.dll");

  base::FilePath inetcpl;
  PathService::Get(base::DIR_SYSTEM, &inetcpl);
  inetcpl = inetcpl.AppendASCII("inetcpl.cpl,,4");

  std::wstring args(shell32dll.value());
  args.append(L",Control_RunDLL ");
  args.append(inetcpl.value());

  ShellExecute(NULL, L"open", rundll32.value().c_str(), args.c_str(), NULL,
               SW_SHOWNORMAL);
}

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
      WebContents* web_contents) {
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&OpenConnectionDialogCallback));
}

void AdvancedOptionsUtilities::ShowManageSSLCertificates(
      WebContents* web_contents) {
  CRYPTUI_CERT_MGR_STRUCT cert_mgr = { 0 };
  cert_mgr.dwSize = sizeof(CRYPTUI_CERT_MGR_STRUCT);
  cert_mgr.hwndParent = views::HWNDForNativeWindow(
      web_contents->GetTopLevelNativeWindow());
  ::CryptUIDlgCertMgr(&cert_mgr);
}

}  // namespace options
