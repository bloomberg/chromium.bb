// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker.h"

#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/download/save_package.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

DownloadFilePicker::DownloadFilePicker(
    DownloadManager* download_manager,
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* params)
    : download_manager_(download_manager) {
  select_file_dialog_ = SelectFileDialog::Create(this);
  SelectFileDialog::FileTypeInfo file_type_info;
  FilePath::StringType extension = suggested_path.Extension();
  if (!extension.empty()) {
    extension.erase(extension.begin());  // drop the .
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(extension);
  }
  file_type_info.include_all_files = true;
  gfx::NativeWindow owning_window = tab_contents ?
      platform_util::GetTopLevel(tab_contents->GetNativeView()) : NULL;

  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                  string16(),
                                  suggested_path,
                                  &file_type_info, 0, FILE_PATH_LITERAL(""),
                                  tab_contents, owning_window, params);
}

DownloadFilePicker::~DownloadFilePicker() {
}

void DownloadFilePicker::ModelChanged() {
}

void DownloadFilePicker::ManagerGoingDown() {
  download_manager_ = NULL;
}

void DownloadFilePicker::FileSelected(const FilePath& path,
                                      int index,
                                      void* params) {
  if (download_manager_)
    download_manager_->FileSelected(path, params);
  delete this;
}

void DownloadFilePicker::FileSelectionCanceled(void* params) {
  if (download_manager_)
    download_manager_->FileSelectionCanceled(params);
  delete this;
}
