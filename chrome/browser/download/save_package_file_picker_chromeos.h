// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {
struct SelectedFileInfo;
}

// Handles showing a dialog to the user to ask for the filename to save a page
// on ChromeOS.
class SavePackageFilePickerChromeOS : public ui::SelectFileDialog::Listener,
                                      public content::WebContentsObserver {
 public:
  SavePackageFilePickerChromeOS(
      content::WebContents* web_contents,
      const base::FilePath& suggested_path,
      bool is_html,
      const content::SavePackagePathPickedCallback& callback);

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

 private:
  virtual ~SavePackageFilePickerChromeOS();

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& selected_path,
                            int unused_index,
                            void* unused_params) OVERRIDE;
  virtual void FileSelectedWithExtraInfo(
      const ui::SelectedFileInfo& selected_file_info,
      int unused_index,
      void* unused_params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  content::SavePackagePathPickedCallback callback_;
  bool is_html_;

  // For managing select file dialogs.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  base::FilePath selected_path_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePickerChromeOS);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_CHROMEOS_H_
