// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class Browser;
class FilePath;

namespace file_handler {

class FileSelector {
 public:
  virtual ~FileSelector() {}

  // Initiate file selection.
  virtual void SelectFile(const FilePath& suggested_name, Browser* browser) = 0;
};

}  // namespace file_handler

class FileHandlerSelectFileFunction : public AsyncExtensionFunction {
 public:
  FileHandlerSelectFileFunction();
  virtual ~FileHandlerSelectFileFunction() OVERRIDE;

  // Called by FileSelector implementation when the user selects new file's
  // file path.
  void OnFilePathSelected(bool success, const FilePath& full_path);

  // Used in test.
  void set_file_selector_for_test(file_handler::FileSelector* file_selector) {
      file_selector_ = file_selector;
  }

  // Used in test.
  void set_render_process_host_id_for_test(int render_process_host_id) {
    render_process_host_id_ = render_process_host_id;
  }

 protected:
  virtual bool RunImpl() OVERRIDE;

 private:
  typedef base::Callback<void(bool, const FilePath&)> CreateFileCallback;

  // Calls |DoCreateFile| on file thread and invokes the callback.
  void CreateFileOnFileThread(const FilePath& full_path,
                              const CreateFileCallback& callback);

  // Creates file on provided file path.
  bool DoCreateFile(const FilePath& full_path);

  // Called on UI thread after the file gets created.
  void OnFileCreated(bool success, const FilePath& full_path);

  // Grants file access permissions for the created file to the extension with
  // cros mount point provider and child process security policy.
  void GrantPermissions(const FilePath& full_path);

  // Sends response to the extension.
  void Respond(bool success, const FilePath& full_path);

  // |file_selector_| and |render_process_host_id_| are used primary in testing
  // to override file selector and render process id that would normally be
  // created (or determined) by the extension function.
  file_handler::FileSelector* file_selector_;
  int render_process_host_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserHandler.selectFile");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_

