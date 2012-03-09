// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
#pragma once

#include "chrome/browser/ui/select_file_dialog.h"

class FilePath;

namespace content {
class DownloadManager;
class WebContents;
}

// Handles showing a dialog to the user to ask for the filename for a download.
class DownloadFilePicker : public SelectFileDialog::Listener {
 public:
  DownloadFilePicker(content::DownloadManager* download_manager,
                     content::WebContents* web_contents,
                     const FilePath& suggested_path,
                     int32 download_id);
  virtual ~DownloadFilePicker();

 protected:
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

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
