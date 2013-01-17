// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_

#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class FilePath;

namespace content {
class DownloadItem;
class DownloadManager;
class WebContents;
}

// Handles showing a dialog to the user to ask for the filename for a download.
class DownloadFilePicker : public ui::SelectFileDialog::Listener {
 public:
  DownloadFilePicker();
  virtual ~DownloadFilePicker();

  void Init(content::DownloadManager* download_manager,
            content::DownloadItem* item,
            const FilePath& suggested_path,
            const ChromeDownloadManagerDelegate::FileSelectedCallback&
                callback);

 protected:
  // On ChromeOS |suggested_path| might be a temporary local filename. This
  // method should be overridden to set the correct suggested path to prompt the
  // user.
  virtual void InitSuggestedPath(content::DownloadItem* item,
                                 const FilePath& suggested_path);

  void set_suggested_path(const FilePath& suggested_path) {
    suggested_path_ = suggested_path;
  }

  // Runs |file_selected_callback_| with |path| and then deletes this object.
  void OnFileSelected(const FilePath& path);

  void RecordFileSelected(const FilePath& path);

  scoped_refptr<content::DownloadManager> download_manager_;
  int32 download_id_;

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  FilePath suggested_path_;

  ChromeDownloadManagerDelegate::FileSelectedCallback file_selected_callback_;

  // For managing select file dialogs.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
