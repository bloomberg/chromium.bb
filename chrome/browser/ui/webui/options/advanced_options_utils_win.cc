// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/advanced_options_utils.h"

#include <windows.h>
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#include <shellapi.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"

// Callback that opens the Internet Options control panel dialog with the
// Connections tab selected.
void OpenConnectionDialogCallback() {
  // Using rundll32 seems better than LaunchConnectionDialog which causes a
  // new dialog to be made for each call.  rundll32 uses the same global
  // dialog and it seems to share with the shortcut in control panel.
  FilePath rundll32;
  PathService::Get(base::DIR_SYSTEM, &rundll32);
  rundll32 = rundll32.AppendASCII("rundll32.exe");

  FilePath shell32dll;
  PathService::Get(base::DIR_SYSTEM, &shell32dll);
  shell32dll = shell32dll.AppendASCII("shell32.dll");

  FilePath inetcpl;
  PathService::Get(base::DIR_SYSTEM, &inetcpl);
  inetcpl = inetcpl.AppendASCII("inetcpl.cpl,,4");

  std::wstring args(shell32dll.value());
  args.append(L",Control_RunDLL ");
  args.append(inetcpl.value());

  ShellExecute(NULL, L"open", rundll32.value().c_str(), args.c_str(), NULL,
               SW_SHOWNORMAL);
}

void AdvancedOptionsUtilities::ShowNetworkProxySettings(
      TabContents* tab_contents) {
  base::Thread* thread = g_browser_process->file_thread();
  DCHECK(thread);
  thread->message_loop()->PostTask(FROM_HERE,
                                   base::Bind(&OpenConnectionDialogCallback));
}

void AdvancedOptionsUtilities::ShowManageSSLCertificates(
      TabContents* tab_contents) {
  CRYPTUI_CERT_MGR_STRUCT cert_mgr = { 0 };
  cert_mgr.dwSize = sizeof(CRYPTUI_CERT_MGR_STRUCT);
  cert_mgr.hwndParent =
#if defined(USE_AURA)
      NULL;
#else
      tab_contents->view()->GetTopLevelNativeWindow();
#endif
  ::CryptUIDlgCertMgr(&cert_mgr);
}
