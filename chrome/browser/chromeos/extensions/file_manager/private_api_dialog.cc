// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_dialog.h"

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "content/public/browser/browser_thread.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::BrowserThread;

namespace extensions {

CancelFileDialogFunction::CancelFileDialogFunction() {
}

CancelFileDialogFunction::~CancelFileDialogFunction() {
}

bool CancelFileDialogFunction::RunImpl() {
  int32 tab_id = file_manager::util::GetTabId(dispatcher());
  SelectFileDialogExtension::OnFileSelectionCanceled(tab_id);
  SendResponse(true);
  return true;
}

SelectFileFunction::SelectFileFunction() {
}

SelectFileFunction::~SelectFileFunction() {
}

bool SelectFileFunction::RunImpl() {
  if (args_->GetSize() != 4) {
    return false;
  }
  std::string file_url;
  args_->GetString(0, &file_url);
  std::vector<GURL> file_paths;
  file_paths.push_back(GURL(file_url));
  bool for_opening = false;
  args_->GetBoolean(2, &for_opening);
  bool need_local_path = false;
  args_->GetBoolean(3, &need_local_path);

  file_manager::util::GetSelectedFileInfoLocalPathOption option =
      file_manager::util::NO_LOCAL_PATH_RESOLUTION;
  if (need_local_path) {
    option = for_opening ?
        file_manager::util::NEED_LOCAL_PATH_FOR_OPENING :
        file_manager::util::NEED_LOCAL_PATH_FOR_SAVING;
  }

  file_manager::util::GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_paths,
      option,
      base::Bind(&SelectFileFunction::GetSelectedFileInfoResponse, this));
  return true;
}

void SelectFileFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  int index;
  args_->GetInteger(1, &index);
  int32 tab_id = file_manager::util::GetTabId(dispatcher());
  SelectFileDialogExtension::OnFileSelected(tab_id, files[0], index);
  SendResponse(true);
}

SelectFilesFunction::SelectFilesFunction() {
}

SelectFilesFunction::~SelectFilesFunction() {
}

bool SelectFilesFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  std::vector<GURL> file_urls;
  file_urls.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    file_urls.push_back(GURL(virtual_path));
  }

  bool need_local_path = false;
  args_->GetBoolean(1, &need_local_path);

  file_manager::util::GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_urls,
      need_local_path ?
          file_manager::util::NEED_LOCAL_PATH_FOR_OPENING :
          file_manager::util::NO_LOCAL_PATH_RESOLUTION,
      base::Bind(&SelectFilesFunction::GetSelectedFileInfoResponse, this));
  return true;
}

void SelectFilesFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int32 tab_id = file_manager::util::GetTabId(dispatcher());
  SelectFileDialogExtension::OnMultiFilesSelected(tab_id, files);
  SendResponse(true);
}

}  // namespace extensions
