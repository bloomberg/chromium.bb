// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace gdata {
class GDataFileSystem;
}

// Handles showing a dialog to the user to ask for the filename to save a page
// on ChromeOS.
class SavePackageFilePickerChromeOS : public SelectFileDialog::Listener,
                                      public content::WebContentsObserver {
 public:
  SavePackageFilePickerChromeOS(
      content::WebContents* web_contents,
      const FilePath& suggested_path,
      const content::SavePackagePathPickedCallback& callback);

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

 private:
  virtual ~SavePackageFilePickerChromeOS();

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int unused_index,
                            void* unused_params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  content::SavePackagePathPickedCallback callback_;

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  FilePath selected_path_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
