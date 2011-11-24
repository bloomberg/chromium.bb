// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#include <windows.h>
#include <shellapi.h>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"

namespace printing {

// A helper callback that opens the printer management dialog.
void OpenPrintersDialogCallback() {
  FilePath sys_dir;
  PathService::Get(base::DIR_SYSTEM, &sys_dir);
  FilePath rundll32 = sys_dir.AppendASCII("rundll32.exe");
  FilePath shell32dll = sys_dir.AppendASCII("shell32.dll");

  std::wstring args(shell32dll.value());
  args.append(L",SHHelpShortcuts_RunDLL PrintersFolder");
  ShellExecute(NULL, L"open", rundll32.value().c_str(), args.c_str(), NULL,
               SW_SHOWNORMAL);
}

void PrinterManagerDialog::ShowPrinterManagerDialog() {
  g_browser_process->file_thread()->message_loop()->PostTask(
      FROM_HERE, base::Bind(OpenPrintersDialogCallback));
}

}  // namespace printing
