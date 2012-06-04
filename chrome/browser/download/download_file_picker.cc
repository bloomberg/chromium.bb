// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::DownloadManager;
using content::WebContents;

namespace {

enum FilePickerResult {
  FILE_PICKER_SAME,
  FILE_PICKER_DIFFERENT_DIR,
  FILE_PICKER_DIFFERENT_NAME,
  FILE_PICKER_CANCEL,
  FILE_PICKER_MAX,
};

// Record how the File Chooser was used during a download. Only record this
// for profiles that do not always prompt for save locations on downloads.
void RecordFilePickerResult(DownloadManager* manager,
                            FilePickerResult result) {
  if (!manager)
    return;
  const DownloadPrefs* prefs = DownloadPrefs::FromDownloadManager(manager);
  if (!prefs)
    return;
  if (prefs->PromptForDownload())
    return;
  UMA_HISTOGRAM_ENUMERATION("Download.FilePickerResult",
                            result,
                            FILE_PICKER_MAX);
}

FilePickerResult ComparePaths(const FilePath& suggested_path,
                              const FilePath& actual_path) {
  if (suggested_path == actual_path)
    return FILE_PICKER_SAME;
  if (suggested_path.DirName() != actual_path.DirName())
    return FILE_PICKER_DIFFERENT_DIR;
  return FILE_PICKER_DIFFERENT_NAME;
}

}  // namespace

DownloadFilePicker::DownloadFilePicker(
    DownloadManager* download_manager,
    WebContents* web_contents,
    const FilePath& suggested_path,
    int32 download_id)
    : download_manager_(download_manager),
      download_id_(download_id),
      suggested_path_(suggested_path) {
  DCHECK(download_manager_);
  select_file_dialog_ = SelectFileDialog::Create(this);
  SelectFileDialog::FileTypeInfo file_type_info;
  FilePath::StringType extension = suggested_path.Extension();
  if (!extension.empty()) {
    extension.erase(extension.begin());  // drop the .
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(extension);
  }
  file_type_info.include_all_files = true;
  gfx::NativeWindow owning_window = web_contents ?
      platform_util::GetTopLevel(web_contents->GetNativeView()) : NULL;

  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                                  string16(),
                                  suggested_path,
                                  &file_type_info, 0, FILE_PATH_LITERAL(""),
                                  web_contents, owning_window, NULL);
}

DownloadFilePicker::~DownloadFilePicker() {
}

void DownloadFilePicker::RecordFileSelected(const FilePath& path) {
  FilePickerResult result = ComparePaths(suggested_path_, path);
  RecordFilePickerResult(download_manager_, result);
}

void DownloadFilePicker::FileSelected(const FilePath& path,
                                      int index,
                                      void* params) {
  RecordFileSelected(path);

  if (download_manager_)
    download_manager_->FileSelected(path, download_id_);
  delete this;
}

void DownloadFilePicker::FileSelectionCanceled(void* params) {
  RecordFilePickerResult(download_manager_, FILE_PICKER_CANCEL);
  if (download_manager_)
    download_manager_->FileSelectionCanceled(download_id_);
  delete this;
}
