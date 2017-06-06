// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace media_router {

class MediaRouterFileDialog : public ui::SelectFileDialog::Listener {
 public:
  // The reasons that the file selection might have failed. Passed into the
  // failure callback.
  enum FailureReason {
    // User canceled dialog, probably most common reason.
    CANCELED,
    // The file is invalid, it does not pass the validity checks.
    INVALID_FILE,
    // The reason for the failure is unknown.
    UNKNOWN,
  };

  class MediaRouterFileDialogDelegate {
   public:
    virtual ~MediaRouterFileDialogDelegate() {}

    // Called when a file is selected by the user to store the files information
    // and tell the message handler to pass along the information.
    virtual void FileDialogFileSelected(
        const ui::SelectedFileInfo& file_info) = 0;

    // Called when the file selection fails for whatever reason, including user
    // cancelation.
    virtual void FileDialogSelectionFailed(FailureReason) = 0;
  };

  explicit MediaRouterFileDialog(MediaRouterFileDialogDelegate* delegate);
  ~MediaRouterFileDialog() override;

  GURL GetLastSelectedFileUrl();
  base::string16 GetLastSelectedFileName();
  base::string16 GetDetailedErrorMessage();

  // Opens the dialog configured to get a media file.
  void OpenFileDialog(Browser* browser);

  // Overridden from SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file_info,
                                 int index,
                                 void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  MediaRouterFileDialogDelegate* delegate_;

  // Pointer to the file last indicated by the system.
  base::Optional<ui::SelectedFileInfo> selected_file_;

  // A string that stores additional error information
  // TODO(offenwanger): Set this when a file is invalid.
  base::Optional<base::string16> detailed_error_message_;

  // The dialog object for the file dialog.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_FILE_DIALOG_H_
