// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/shell_handler_win.h"

#include <commdlg.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/utility/utility_thread.h"
#include "ui/base/win/open_file_name_win.h"

ShellHandler::ShellHandler() {}
ShellHandler::~ShellHandler() {}

bool ShellHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetOpenFileName,
                        OnGetOpenFileName)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ShellHandler::OnGetOpenFileName(
    HWND owner,
    DWORD flags,
    const GetOpenFileNameFilter& filter,
    const base::FilePath& initial_directory,
    const base::FilePath& filename) {
  ui::win::OpenFileName open_file_name(owner, flags);
  open_file_name.SetInitialSelection(initial_directory, filename);
  open_file_name.SetFilters(filter);

  base::FilePath directory;
  std::vector<base::FilePath> filenames;

  if (::GetOpenFileName(open_file_name.GetOPENFILENAME()))
    open_file_name.GetResult(&directory, &filenames);

  if (filenames.size()) {
    content::UtilityThread::Get()->Send(
        new ChromeUtilityHostMsg_GetOpenFileName_Result(directory, filenames));
  } else {
    content::UtilityThread::Get()->Send(
        new ChromeUtilityHostMsg_GetOpenFileName_Failed());
  }
}
