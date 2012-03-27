// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
#pragma once

#include "base/platform_file.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/public/browser/web_contents_observer.h"

namespace gdata {
class GDataFileSystem;
}

// Handles showing a dialog to the user to ask for the filename to save a page
// on ChromeOS.
class SavePackageFilePickerChromeOS : public SelectFileDialog::Listener,
                                      public content::WebContentsObserver {
 public:
  SavePackageFilePickerChromeOS(content::WebContents* web_contents,
                                const FilePath& suggested_path);

 private:
  virtual ~SavePackageFilePickerChromeOS();

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // Calls WebContent::GenerateMHTML.
  void GenerateMHTML(const FilePath* mhtml_path);

  // Callback for WebContents::GenerateMHTML.
  void OnMHTMLGenerated(const FilePath& file_path, int64 file_size);

  // GDataFileSystem::TransferFile callback.
  void OnTransferFile(base::PlatformFileError error);

  // Use web_contents() to get at GDataFileSystem.
  gdata::GDataFileSystem* GetGDataFileSystem();

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  FilePath selected_path_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
