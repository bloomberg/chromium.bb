// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides API functions for the file manager to act as the file
// dialog for opening and saving files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_

#include <vector>

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

namespace ui {
struct SelectedFileInfo;
}

namespace extensions {

// Cancel file selection Dialog.  Closes the dialog window.
class FileBrowserPrivateCancelDialogFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelDialog",
                             FILEBROWSERPRIVATE_CANCELDIALOG)

 protected:
  virtual ~FileBrowserPrivateCancelDialogFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class FileBrowserPrivateSelectFileFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFile",
                             FILEBROWSERPRIVATE_SELECTFILE)

 protected:
  virtual ~FileBrowserPrivateSelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      int index,
      const std::vector<ui::SelectedFileInfo>& files);
};

// Select multiple files.  Closes the dialog window.
class FileBrowserPrivateSelectFilesFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFiles",
                             FILEBROWSERPRIVATE_SELECTFILES)

 protected:
  virtual ~FileBrowserPrivateSelectFilesFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      const std::vector<ui::SelectedFileInfo>& files);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_
