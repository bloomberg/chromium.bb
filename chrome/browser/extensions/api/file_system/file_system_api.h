// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class FileSystemGetDisplayPathFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getDisplayPath");

 protected:
  virtual ~FileSystemGetDisplayPathFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemPickerFunction : public AsyncExtensionFunction {
 protected:
  class FilePicker;

  virtual ~FileSystemPickerFunction() {}
  bool ShowPicker(const FilePath& suggested_path, bool for_save);

 private:
  // FileSelected and FileSelectionCanceled are called by the file picker.
  void FileSelected(const FilePath& path, bool for_save);
  void FileSelectionCanceled();

  // called on the FILE thread. This is only called when a file is being chosen
  // to save. The function will ensure the file exists, creating it if
  // necessary, and also check that the file is not a link.
  void CheckWritableFile(const FilePath& path);

  // This will finish the choose file process. This is either called directly
  // from FileSelected, or from CreateFileIfNecessary. It is called on the UI
  // thread.
  void RegisterFileSystemAndSendResponse(const FilePath& path, bool for_save);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError();
};

class FileSystemGetWritableFileEntryFunction : public FileSystemPickerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getWritableFileEntry");

 protected:
  virtual ~FileSystemGetWritableFileEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemChooseFileFunction : public FileSystemPickerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.chooseFile");

 protected:
  virtual ~FileSystemChooseFileFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
