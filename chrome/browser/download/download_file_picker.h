// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/download/download_confirmation_result.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
}

namespace download {
class DownloadItem;
}

// Handles showing a dialog to the user to ask for the filename for a download.
class DownloadFilePicker : public ui::SelectFileDialog::Listener {
 public:
  // Callback used to pass the user selection back to the owner of this
  // object.
  // |virtual_path|: The path chosen by the user. If the user cancels the file
  //    selection, then this parameter will be the empty path. On Chrome OS,
  //    this path may contain virtual mount points if the user chose a virtual
  //    path (e.g. Google Drive).
  typedef base::Callback<void(DownloadConfirmationResult,
                              const base::FilePath& virtual_path)>
      ConfirmationCallback;

  // Display a file picker dialog for |item|. The |suggested_path| will be used
  // as the initial path displayed to the user. |callback| will always be
  // invoked even if |item| is destroyed prior to the file picker completing.
  static void ShowFilePicker(download::DownloadItem* item,
                             const base::FilePath& suggested_path,
                             const ConfirmationCallback& callback);

 private:
  DownloadFilePicker(download::DownloadItem* item,
                     const base::FilePath& suggested_path,
                     const ConfirmationCallback& callback);
  ~DownloadFilePicker() override;

  // Runs |file_selected_callback_| with |virtual_path| and then deletes this
  // object.
  void OnFileSelected(const base::FilePath& virtual_path);

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // Initially suggested path.
  base::FilePath suggested_path_;

  // Callback invoked when a file selection is complete.
  ConfirmationCallback file_selected_callback_;

  // For managing select file dialogs.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
