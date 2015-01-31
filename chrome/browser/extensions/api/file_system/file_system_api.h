// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/file_system.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace extensions {
class ExtensionPrefs;

namespace file_system_api {

// Methods to get and set the path of the directory containing the last file
// chosen by the user in response to a chrome.fileSystem.chooseEntry() call for
// the given extension.

// Returns an empty path on failure.
base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const std::string& extension_id);

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 const base::FilePath& path);

std::vector<base::FilePath> GetGrayListedDirectories();

}  // namespace file_system_api

class FileSystemGetDisplayPathFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getDisplayPath",
                             FILESYSTEM_GETDISPLAYPATH)

 protected:
  ~FileSystemGetDisplayPathFunction() override {}
  bool RunSync() override;
};

class FileSystemEntryFunction : public ChromeAsyncExtensionFunction {
 protected:
  FileSystemEntryFunction();

  ~FileSystemEntryFunction() override {}

  // This is called when writable file entries are being returned. The function
  // will ensure the files exist, creating them if necessary, and also check
  // that none of the files are links. If it succeeds it proceeds to
  // RegisterFileSystemsAndSendResponse, otherwise to HandleWritableFileError.
  void PrepareFilesForWritableApp(const std::vector<base::FilePath>& path);

  // This will finish the choose file process. This is either called directly
  // from FilesSelected, or from WritableFileChecker. It is called on the UI
  // thread.
  void RegisterFileSystemsAndSendResponse(
      const std::vector<base::FilePath>& path);

  // Creates a response dictionary and sets it as the response to be sent.
  void CreateResponse();

  // Adds an entry to the response dictionary.
  void AddEntryToResponse(const base::FilePath& path,
                          const std::string& id_override);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError(const base::FilePath& error_path);

  // Whether multiple entries have been requested.
  bool multiple_;

  // Whether a directory has been requested.
  bool is_directory_;

  // The dictionary to send as the response.
  base::DictionaryValue* response_;
};

class FileSystemGetWritableEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getWritableEntry",
                             FILESYSTEM_GETWRITABLEENTRY)

 protected:
  ~FileSystemGetWritableEntryFunction() override {}
  bool RunAsync() override;

 private:
  void CheckPermissionAndSendResponse();
  void SetIsDirectoryOnFileThread();

  // The path to the file for which a writable entry has been requested.
  base::FilePath path_;
};

class FileSystemIsWritableEntryFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isWritableEntry",
                             FILESYSTEM_ISWRITABLEENTRY)

 protected:
  ~FileSystemIsWritableEntryFunction() override {}
  bool RunSync() override;
};

class FileSystemChooseEntryFunction : public FileSystemEntryFunction {
 public:
  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(base::FilePath* path);
  static void SkipPickerAndAlwaysSelectPathsForTest(
      std::vector<base::FilePath>* paths);
  static void SkipPickerAndSelectSuggestedPathForTest();
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();
  // Allow directory access confirmation UI to be skipped in testing.
  static void SkipDirectoryConfirmationForTest();
  static void AutoCancelDirectoryConfirmationForTest();
  static void StopSkippingDirectoryConfirmationForTest();
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

  ~FileSystemChooseEntryFunction() override {}
  bool RunAsync() override;
  void ShowPicker(const ui::SelectFileDialog::FileTypeInfo& file_type_info,
                  ui::SelectFileDialog::Type picker_type);

 private:
  void SetInitialPathOnFileThread(const base::FilePath& suggested_name,
                                  const base::FilePath& previous_path);

  // FilesSelected and FileSelectionCanceled are called by the file picker.
  void FilesSelected(const std::vector<base::FilePath>& path);
  void FileSelectionCanceled();

  // Check if |check_path|, the canonicalized form of the chosen directory
  // |paths|, is or is an ancestor of a sensitive directory. If so, show a
  // dialog to confirm that the user wants to open the directory.
  // Calls OnDirectoryAccessConfirmed if the directory isn't sensitive or the
  // user chooses to open it. Otherwise, calls FileSelectionCanceled.
  void ConfirmDirectoryAccessOnFileThread(
      const base::FilePath& check_path,
      const std::vector<base::FilePath>& paths,
      content::WebContents* web_contents);
  void OnDirectoryAccessConfirmed(const std::vector<base::FilePath>& paths);

  base::FilePath initial_path_;
};

class FileSystemRetainEntryFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.retainEntry", FILESYSTEM_RETAINENTRY)

 protected:
  ~FileSystemRetainEntryFunction() override {}
  bool RunAsync() override;

 private:
  // Retains the file entry referenced by |entry_id| in apps::SavedFilesService.
  // |entry_id| must refer to an entry in an isolated file system.  |path| is a
  // path of the entry.  |file_info| is base::File::Info of the entry if it can
  // be obtained.
  void RetainFileEntry(const std::string& entry_id,
                       const base::FilePath& path,
                       scoped_ptr<base::File::Info> file_info);
};

class FileSystemIsRestorableFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isRestorable", FILESYSTEM_ISRESTORABLE)

 protected:
  ~FileSystemIsRestorableFunction() override {}
  bool RunSync() override;
};

class FileSystemRestoreEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.restoreEntry", FILESYSTEM_RESTOREENTRY)

 protected:
  ~FileSystemRestoreEntryFunction() override {}
  bool RunAsync() override;
};

class FileSystemObserveDirectoryFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.observeDirectory",
                             FILESYSTEM_OBSERVEDIRECTORY)

 protected:
  ~FileSystemObserveDirectoryFunction() override {}
  bool RunSync() override;
};

class FileSystemUnobserveEntryFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.unobserveEntry",
                             FILESYSTEM_UNOBSERVEENTRY)

 protected:
  ~FileSystemUnobserveEntryFunction() override {}
  bool RunSync() override;
};

class FileSystemGetObservedEntriesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getObservedEntries",
                             FILESYSTEM_GETOBSERVEDENTRIES);

 protected:
  ~FileSystemGetObservedEntriesFunction() override {}
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
