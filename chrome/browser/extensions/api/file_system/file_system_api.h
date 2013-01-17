// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/file_system.h"
#include "ui/shell_dialogs/select_file_dialog.h"

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

class FileSystemGetWritableEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getWritableEntry");

 protected:
  virtual ~FileSystemGetWritableEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemIsWritableEntryFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.isWritableEntry");

 protected:
  virtual ~FileSystemIsWritableEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemChooseEntryFunction : public FileSystemEntryFunction {
 public:
  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(FilePath* path);
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();

  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.chooseEntry");

  typedef std::vector<linked_ptr<extensions::api::file_system::AcceptOption> >
      AcceptOptions;

  static void BuildFileTypeInfo(
      ui::SelectFileDialog::FileTypeInfo* file_type_info,
      const FilePath::StringType& suggested_extension,
      const AcceptOptions* accepts,
      const bool* acceptsAllTypes);
  static void BuildSuggestion(const std::string* opt_name,
                              FilePath* suggested_name,
                              FilePath::StringType* suggested_extension);

 protected:
  class FilePicker;

  virtual ~FileSystemChooseEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
  bool ShowPicker(const FilePath& suggested_path,
                  const ui::SelectFileDialog::FileTypeInfo& file_type_info,
                  ui::SelectFileDialog::Type picker_type,
                  EntryType entry_type);

 private:
  // FileSelected and FileSelectionCanceled are called by the file picker.
  void FileSelected(const FilePath& path, EntryType entry_type);
  void FileSelectionCanceled();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
