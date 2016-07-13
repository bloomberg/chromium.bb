// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/ipc_shell_handler_win.h"

#include <commdlg.h>

#include <memory>

#include "base/files/file_path.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/utility/utility_thread.h"
#include "ui/base/win/open_file_name_win.h"

IPCShellHandler::IPCShellHandler() {}
IPCShellHandler::~IPCShellHandler() {}

bool IPCShellHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IPCShellHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetOpenFileName, OnGetOpenFileName)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetSaveFileName, OnGetSaveFileName)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IPCShellHandler::OnGetOpenFileName(HWND owner,
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

void IPCShellHandler::OnGetSaveFileName(
    const ChromeUtilityMsg_GetSaveFileName_Params& params) {
  ui::win::OpenFileName open_file_name(params.owner, params.flags);
  open_file_name.SetInitialSelection(params.initial_directory,
                                     params.suggested_filename);
  open_file_name.SetFilters(params.filters);
  open_file_name.GetOPENFILENAME()->nFilterIndex =
      params.one_based_filter_index;
  open_file_name.GetOPENFILENAME()->lpstrDefExt =
      params.default_extension.c_str();

  if (::GetSaveFileName(open_file_name.GetOPENFILENAME())) {
    content::UtilityThread::Get()->Send(
        new ChromeUtilityHostMsg_GetSaveFileName_Result(
            base::FilePath(open_file_name.GetOPENFILENAME()->lpstrFile),
            open_file_name.GetOPENFILENAME()->nFilterIndex));
    return;
  }

  // Zero means the dialog was closed, otherwise we had an error.
  DWORD error_code = ::CommDlgExtendedError();
  if (error_code != 0)
    NOTREACHED() << "GetSaveFileName failed with code: " << error_code;

  content::UtilityThread::Get()->Send(
      new ChromeUtilityHostMsg_GetSaveFileName_Failed());
}
