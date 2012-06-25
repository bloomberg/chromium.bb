// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/extensions/extension_function.h"

class Browser;
class FileHandlerSelectFileFunction;

namespace file_handler {

class FileSelector {
 public:
  virtual ~FileSelector() {}

  // Initiate file selection.
  virtual void SelectFile(const FilePath& suggested_name, Browser* browser) = 0;

  // Used in testing.
  virtual void set_function_for_test(
      FileHandlerSelectFileFunction* function) = 0;
};

}  // namespace file_handler

class FileHandlerSelectFileFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserHandlerInternal.selectFile");

  FileHandlerSelectFileFunction();

  // Called by FileSelector implementation when the user selects new file's
  // file path.
  void OnFilePathSelected(bool success, const FilePath& full_path);

  // Used in test.
  static void set_file_selector_for_test(
      file_handler::FileSelector* file_selector);

  // Used in test.
  static void set_gesture_check_disabled_for_test(bool disabled);

 protected:
  virtual ~FileHandlerSelectFileFunction() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;

 private:
  // Calls |DoCreateFile| on file thread and invokes the callback.
  void CreateFileOnFileThread(bool success,
                              const std::string& file_system_name,
                              const GURL& file_system_root);

  // Creates file on provided file path.
  bool DoCreateFile();

  // Called on UI thread after the file gets created.
  void OnFileCreated(bool success,
                     const std::string& file_system_name,
                     const GURL& file_system_root);

  // Grants file access permissions for the created file to the extension with
  // cros mount point provider and child process security policy.
  // Returns virtual path for which has been given permission.
  FilePath GrantPermissions();

  // Sends response to the extension.
  void Respond(bool success,
               const std::string& file_system_name,
               const GURL& file_system_root,
               const FilePath& virtual_path);

  // Gets file selector that should be used to select the file.
  // We should not take the ownership of the selector.
  file_handler::FileSelector* GetFileSelector();

  // Full file system path of the selected file.
  FilePath full_path_;

  // |file_selector_for_test_| and |disable_geture_check_for_test_| are used
  // primary in testing to override file selector to be used in the function
  // implementation and disable user gesture check.
  // Once set they will be used for every extension function call.
  static file_handler::FileSelector* file_selector_for_test_;
  static bool gesture_check_disabled_for_test_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_

