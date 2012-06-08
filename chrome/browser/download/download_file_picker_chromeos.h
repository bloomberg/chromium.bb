// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
#pragma once

#include "chrome/browser/download/download_file_picker.h"

class DownloadFilePickerChromeOS : public DownloadFilePicker {
 public:
  DownloadFilePickerChromeOS();
  virtual ~DownloadFilePickerChromeOS();

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // DownloadFilePicker implementation.
  // This looks up the gdata path instead of the temporary local path.
  virtual void InitSuggestedPath(content::DownloadItem* item) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
