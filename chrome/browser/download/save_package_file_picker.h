// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/public/browser/download_manager_delegate.h"

class DownloadPrefs;

// Handles showing a dialog to the user to ask for the filename to save a page.
class SavePackageFilePicker : public SelectFileDialog::Listener {
 public:
  SavePackageFilePicker(content::WebContents* web_contents,
                        const FilePath& suggested_path,
                        const FilePath::StringType& default_extension,
                        bool can_save_as_complete,
                        DownloadPrefs* download_prefs,
                        const content::SavePackagePathPickedCallback& callback);
  virtual ~SavePackageFilePicker();

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* unused_params) OVERRIDE;
  virtual void FileSelectionCanceled(void* unused_params) OVERRIDE;

  bool ShouldSaveAsMHTML() const;

  // Used to look up the renderer process for this request to get the context.
  int render_process_id_;

  // Whether the web page can be saved as a complete HTML file.
  bool can_save_as_complete_;

  content::SavePackagePathPickedCallback callback_;

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
