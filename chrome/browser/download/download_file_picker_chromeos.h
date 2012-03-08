// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
#pragma once

#include "chrome/browser/download/download_file_picker.h"

class DownloadFilePickerChromeOS : public DownloadFilePicker {
 public:
  DownloadFilePickerChromeOS(content::DownloadManager* download_manager,
                             content::WebContents* web_contents,
                             const FilePath& suggested_path,
                             int32 download_id);
  virtual ~DownloadFilePickerChromeOS();

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_CHROMEOS_H__
