// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__

#include "chrome/browser/download/download_file_picker.h"

namespace ui {
struct SelectedFileInfo;
}

class DownloadFilePickerChromeOS : public DownloadFilePicker {
 public:
  DownloadFilePickerChromeOS();
  virtual ~DownloadFilePickerChromeOS();

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectedWithExtraInfo(
      const ui::SelectedFileInfo& file_info,
      int index,
      void* params) OVERRIDE;

  // DownloadFilePicker implementation.
  // This looks up the gdata path instead of the temporary local path.
  virtual void InitSuggestedPath(content::DownloadItem* item,
                                 const base::FilePath& suggested_path) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
