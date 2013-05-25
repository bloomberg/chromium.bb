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
  DECLARE_EXTENSION_FUNCTION("fileSystem.getDisplayPath",
                             FILESYSTEM_GETDISPLAYPATH)

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

  // This is called when a writable file entry is being returned. The function
  // will ensure the file exists, creating it if necessary, and also check that
  // the file is not a link. If it succeeds it proceeds to
  // RegisterFileSystemAndSendResponse, otherwise to HandleWritableFileError.
  void CheckWritableFile(const base::FilePath& path);

  // This will finish the choose file process. This is either called directly
  // from FileSelected, or from CreateFileIfNecessary. It is called on the UI
  // thread.
  void RegisterFileSystemAndSendResponse(const base::FilePath& path,
                                         EntryType entry_type);

  // This will finish the choose file process. This is either called directly
  // from FileSelected, or from CreateFileIfNecessary. It is called on the UI
  // thread. |id_override| specifies the id to send in the response instead of
  // the generated id. This can be useful for creating a file entry with an id
  // matching another file entry, e.g. for restoreEntry.
  void RegisterFileSystemAndSendResponseWithIdOverride(
      const base::FilePath& path,
      EntryType entry_type,
      const std::string& id_override);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError();
};

class FileSystemGetWritableEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getWritableEntry",
                             FILESYSTEM_GETWRITABLEENTRY)

 protected:
  virtual ~FileSystemGetWritableEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemIsWritableEntryFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isWritableEntry",
                             FILESYSTEM_ISWRITABLEENTRY)

 protected:
  virtual ~FileSystemIsWritableEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemChooseEntryFunction : public FileSystemEntryFunction {
 public:
  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(base::FilePath* path);
  static void SkipPickerAndSelectSuggestedPathForTest();
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();
  // Call this with the directory for test file paths. On Chrome OS, accessed
  // path needs to be explicitly registered for smooth integration with Google
  // Drive support.
  static void RegisterTempExternalFileSystemForTest(const std::string& name,
                                                    const base::FilePath& path);

  DECLARE_EXTENSION_FUNCTION("fileSystem.chooseEntry", FILESYSTEM_CHOOSEENTRY)

  typedef std::vector<linked_ptr<extensions::api::file_system::AcceptOption> >
      AcceptOptions;

  static void BuildFileTypeInfo(
      ui::SelectFileDialog::FileTypeInfo* file_type_info,
      const base::FilePath::StringType& suggested_extension,
      const AcceptOptions* accepts,
      const bool* acceptsAllTypes);
  static void BuildSuggestion(const std::string* opt_name,
                              base::FilePath* suggested_name,
                              base::FilePath::StringType* suggested_extension);

 protected:
  class FilePicker;

  virtual ~FileSystemChooseEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
  void ShowPicker(const ui::SelectFileDialog::FileTypeInfo& file_type_info,
                  ui::SelectFileDialog::Type picker_type,
                  EntryType entry_type);

 private:
  void SetInitialPathOnFileThread(const base::FilePath& suggested_name,
                                  const base::FilePath& previous_path);

  // FileSelected and FileSelectionCanceled are called by the file picker.
  void FileSelected(const base::FilePath& path, EntryType entry_type);
  void FileSelectionCanceled();

  base::FilePath initial_path_;
};

class FileSystemRetainEntryFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.retainEntry", FILESYSTEM_RETAINENTRY)

 protected:
  virtual ~FileSystemRetainEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  // Retains the file entry referenced by |entry_id| in apps::SavedFilesService.
  // |entry_id| must refer to an entry in an isolated file system.
  bool RetainFileEntry(const std::string& entry_id);
};

class FileSystemIsRestorableFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isRestorable", FILESYSTEM_ISRESTORABLE)

 protected:
  virtual ~FileSystemIsRestorableFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemRestoreEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.restoreEntry", FILESYSTEM_RESTOREENTRY)

 protected:
  virtual ~FileSystemRestoreEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
