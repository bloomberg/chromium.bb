// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_file_select_helper.h"

#include "base/file_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/render_messages_params.h"

TabContentsFileSelectHelper::TabContentsFileSelectHelper(
    TabContents* tab_contents)
    : tab_contents_(tab_contents),
      select_file_dialog_(),
      dialog_type_(SelectFileDialog::SELECT_OPEN_FILE) {
}

TabContentsFileSelectHelper::~TabContentsFileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  // Stop any pending directory enumeration and prevent a callback.
  if (directory_lister_.get()) {
    directory_lister_->set_delegate(NULL);
    directory_lister_->Cancel();
  }
}

void TabContentsFileSelectHelper::FileSelected(const FilePath& path,
                                               int index, void* params) {
  tab_contents_->profile()->set_last_selected_directory(path.DirName());

  if (dialog_type_ == SelectFileDialog::SELECT_FOLDER) {
    DirectorySelected(path);
    return;
  }

  std::vector<FilePath> files;
  files.push_back(path);
  GetRenderViewHost()->FilesSelectedInChooser(files);
}

void TabContentsFileSelectHelper::MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
  if (!files.empty())
    tab_contents_->profile()->set_last_selected_directory(files[0].DirName());
  GetRenderViewHost()->FilesSelectedInChooser(files);
}

void TabContentsFileSelectHelper::DirectorySelected(const FilePath& path) {
  directory_lister_ = new net::DirectoryLister(path,
                                               true,
                                               net::DirectoryLister::NO_SORT,
                                               this);
  if (!directory_lister_->Start())
    FileSelectionCanceled(NULL);
}

void TabContentsFileSelectHelper::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
  // Directory upload only cares about files.  This util call just checks
  // the flags in the structure; there's no file I/O going on.
  if (file_util::FileEnumerator::IsDirectory(data.info))
    return;

  directory_lister_results_.push_back(data.path);
}

void TabContentsFileSelectHelper::OnListDone(int error) {
  if (error) {
    FileSelectionCanceled(NULL);
    return;
  }

  GetRenderViewHost()->FilesSelectedInChooser(directory_lister_results_);
  directory_lister_ = NULL;
}

void TabContentsFileSelectHelper::FileSelectionCanceled(void* params) {
  // If the user cancels choosing a file to upload we pass back an
  // empty vector.
  GetRenderViewHost()->FilesSelectedInChooser(std::vector<FilePath>());
}

void TabContentsFileSelectHelper::RunFileChooser(
    const ViewHostMsg_RunFileChooser_Params &params) {
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  switch (params.mode) {
    case ViewHostMsg_RunFileChooser_Params::Open:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case ViewHostMsg_RunFileChooser_Params::OpenMultiple:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case ViewHostMsg_RunFileChooser_Params::OpenFolder:
      dialog_type_ = SelectFileDialog::SELECT_FOLDER;
      break;
    case ViewHostMsg_RunFileChooser_Params::Save:
      dialog_type_ = SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;  // Prevent warning.
      NOTREACHED();
  }
  FilePath default_file_name = params.default_file_name;
  if (default_file_name.empty())
    default_file_name = tab_contents_->profile()->last_selected_directory();
  select_file_dialog_->SelectFile(
      dialog_type_,
      params.title,
      default_file_name,
      NULL,
      0,
      FILE_PATH_LITERAL(""),
      tab_contents_->view()->GetTopLevelNativeWindow(),
      NULL);
}

RenderViewHost* TabContentsFileSelectHelper::GetRenderViewHost() {
  return tab_contents_->render_view_host();
}
