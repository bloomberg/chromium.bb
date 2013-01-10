// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File contains the fileBrowserHandlerInternal.selectFile extension function.
// The function prompts user to select a file path to be used by the caller. It
// will fail if it isn't invoked by a user gesture (e.g. a mouse click or a
// keyboard key press).
// Note that the target file is never actually created by this function, even
// if the selected path doesn't exist.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/extensions/extension_function.h"

class Browser;
class FileHandlerSelectFileFunction;

namespace file_handler {

// Interface that is used by FileHandlerSelectFileFunction to select the file
// path that should be reported back to the extension function caller.
// Nobody will take the ownership of the interface implementation, so it should
// delete itself once it's done.
class FileSelector {
 public:
  virtual ~FileSelector() {}

  // Starts the file selection. It should prompt user to select a file path.
  // Once the selection is made it should asynchronously call
  // |function_->OnFilePathSelected| with the selection information.
  // User should be initially suggested to select file named |suggested_name|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'. This spec comes from
  // ui::SelectFileDialog() which takes extensions without '.'.
  //
  // Selection UI should be displayed using |browser|. |browser| should outlive
  // the interface implementation.
  // |function| if the extension function that called the method and needs to
  // be notified of user action. The interface implementation should keep a
  // reference to the function until it is notified (extension function
  // implementations are ref counted).
  // |SelectFile| will be called at most once by a single extension function.
  // The interface implementation should delete itself after the extension
  // function is notified of file selection result.
  virtual void SelectFile(const FilePath& suggested_name,
                          const std::vector<std::string>& allowed_extensions,
                          Browser* browser,
                          FileHandlerSelectFileFunction* function) = 0;
};

// Interface that is used by FileHandlerSelectFileFunction to create a
// FileSelector it can use to select a file path.
class FileSelectorFactory {
 public:
  virtual ~FileSelectorFactory() {}

  // Creates a FileSelector instance for the FileHandlerSelectFileFunction.
  virtual FileSelector* CreateFileSelector() const = 0;
};

}  // namespace file_handler

// The fileBrowserHandlerInternal.selectFile extension function implementation.
// See the file description for more info.
class FileHandlerSelectFileFunction : public AsyncExtensionFunction {
 public:
  // Default constructor used in production code.
  // It will create its own FileSelectorFactory implementation, and set the
  // value of |user_gesture_check_enabled| to true.
  FileHandlerSelectFileFunction();

  // This constructor should be used only in tests to inject test file selector
  // factory and to allow extension function to run even if it hasn't been
  // invoked by user gesture.
  // Created object will take the ownership of the |file_selector_factory|.
  FileHandlerSelectFileFunction(
      file_handler::FileSelectorFactory* file_selector_factory,
      bool enable_user_gesture_check);

  // Called by FileSelector implementation when the user selects the file's
  // file path. File access permissions for the selected file are granted and
  // caller is notified of the selection result after this method is called.
  // |success| Whether the path was selected.
  // |full_path| The selected file path if one was selected. It is ignored if
  // the selection did not succeed.
  void OnFilePathSelected(bool success, const FilePath& full_path);

 protected:
  // The class is ref counted, so destructor should not be public.
  virtual ~FileHandlerSelectFileFunction() OVERRIDE;

  // AsyncExtensionFunction implementation.
  // Runs the extension function implementation.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Called when the external file system is opened for the extension function
  // caller in the browser context. It saves opened file system's parameters.
  // The file system is needed to create FileEntry object for the selection
  // result.
  // |success| Whether the file system has been opened successfully.
  // |file_system_name| The file system's name.
  // |file_system_root| The file system's root url.
  void OnFileSystemOpened(bool success,
                          const std::string& file_system_name,
                          const GURL& file_system_root);

  // Grants file access permissions for the created file to the caller.
  // Inside this method, |virtual_path_| value is set.
  void GrantPermissions();

  // Callback called when we collect all paths and permissions that should be
  // given to the caller render process in order for it to normally access file.
  // It grants selected permissions with child process security policy.
  void OnGotPermissionsToGrant();

  // Creates dictionary value that will be used to as the extension function's
  // callback argument and ends extension function execution by calling
  // |SendResponse(true)|.
  // The |results_| value will be set to dictionary containing two properties:
  // * boolean 'success', which will be equal to |success|.
  // * object 'entry', which will be set only when |success| if true and
  //   will contain information needed to create a FileEntry object for the
  //   selected file. It contains following properties:
  //   * 'file_system_name' set to |file_system_name_|
  //   * 'file_system_root' set to |file_system_root_|
  //   * 'file_full_path' set to |virtual_path_| (with leading '/')
  //   * 'file_is_directory' set to |false|.
  //   |file_system_name_|, |file_system_root_| and |virtual_path_| are ignored
  //   if |success| if false.
  void Respond(bool success);

  // Factory used to create FileSelector to be used for prompting user to select
  // file.
  scoped_ptr<file_handler::FileSelectorFactory> file_selector_factory_;
  // Whether user gesture check is disabled. This should be true only in tests.
  bool user_gesture_check_enabled_;

  // Full file system path of the selected file.
  FilePath full_path_;
  // Selected file's virtual path in extension function caller's file system.
  FilePath virtual_path_;
  // Extension function caller's file system name.
  std::string file_system_name_;
  // Extension function caller's file system root URL.
  GURL file_system_root_;

  // List of permissions and paths that have to be granted for the selected
  // files.
  std::vector<std::pair<FilePath, int> > permissions_to_grant_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserHandlerInternal.selectFile");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_HANDLER_API_H_
