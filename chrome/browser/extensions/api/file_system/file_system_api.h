// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/ui/select_file_dialog.h"

namespace extensions {

class FileSystemGetDisplayPathFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getDisplayPath");

 protected:
  virtual ~FileSystemGetDisplayPathFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemEntryFunction : public AsyncExtensionFunction {
 protected:
  enum EntryType {
    READ_ONLY,
    WRITABLE
  };

  virtual ~FileSystemEntryFunction() {}

  bool HasFileSystemWritePermission();

  // Called on the FILE thread. This is called when a writable file entry is
  // being returned. The function will ensure the file exists, creating it if
  // necessary, and also check that the file is not a link.
  void CheckWritableFile(const FilePath& path);

  // This will finish the choose file process. This is either called directly
  // from FileSelected, or from CreateFileIfNecessary. It is called on the UI
  // thread.
  void RegisterFileSystemAndSendResponse(const FilePath& path,
                                         EntryType entry_type);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError();
};

class FileSystemGetWritableFileEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getWritableFileEntry");

 protected:
  virtual ~FileSystemGetWritableFileEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemChooseFileFunction : public FileSystemEntryFunction {
 public:
  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(FilePath* path);
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();

  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.chooseFile");

 protected:
  class FilePicker;

  virtual ~FileSystemChooseFileFunction() {}
  virtual bool RunImpl() OVERRIDE;
  bool ShowPicker(const FilePath& suggested_path,
                  SelectFileDialog::Type picker_type,
                  EntryType entry_type);

 private:
  // FileSelected and FileSelectionCanceled are called by the file picker.
  void FileSelected(const FilePath& path, EntryType entry_type);
  void FileSelectionCanceled();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
