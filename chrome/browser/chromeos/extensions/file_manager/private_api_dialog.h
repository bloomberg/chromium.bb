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

namespace file_manager {

// Cancel file selection Dialog.  Closes the dialog window.
class CancelFileDialogFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelDialog",
                             FILEBROWSERPRIVATE_CANCELDIALOG)

  CancelFileDialogFunction();

 protected:
  virtual ~CancelFileDialogFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class SelectFileFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFile",
                             FILEBROWSERPRIVATE_SELECTFILE)

  SelectFileFunction();

 protected:
  virtual ~SelectFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      const std::vector<ui::SelectedFileInfo>& files);
};

// Select multiple files.  Closes the dialog window.
class SelectFilesFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.selectFiles",
                             FILEBROWSERPRIVATE_SELECTFILES)

  SelectFilesFunction();

 protected:
  virtual ~SelectFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      const std::vector<ui::SelectedFileInfo>& files);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_
