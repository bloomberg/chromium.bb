// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/select_file_dialog.h"

class DownloadPrefs;
class FilePath;
class SavePackage;

// Handles showing a dialog to the user to ask for the filename to save a page.
class SavePackageFilePicker : public SelectFileDialog::Listener {
 public:
  SavePackageFilePicker(const base::WeakPtr<SavePackage>& save_package,
                        const FilePath& suggested_path,
                        bool can_save_as_complete,
                        DownloadPrefs* download_prefs);
  virtual ~SavePackageFilePicker();

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  base::WeakPtr<SavePackage> save_package_;

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
