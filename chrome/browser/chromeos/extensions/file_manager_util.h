// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "googleurl/src/gurl.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Browser;
class Profile;

namespace base {
class ListValue;
}

extern const char kFileBrowserDomain[];
extern const char kFileBrowserGalleryTaskId[];
extern const char kFileBrowserWatchTaskId[];

// File manager helper methods.
namespace file_manager_util {

// Gets base file browser url.
GURL GetFileBrowserExtensionUrl();
GURL GetFileBrowserUrl();
GURL GetMediaPlayerUrl();
GURL GetVideoPlayerUrl();

// Converts |full_file_path| into external filesystem: url. Returns false
// if |full_file_path| is not managed by the external filesystem provider.
bool ConvertFileToFileSystemUrl(Profile* profile,
                                const base::FilePath& full_file_path,
                                const std::string& extension_id,
                                GURL* url);

// Converts |full_file_path| into |relative_path| within the external provider
// in File API. Returns false if |full_file_path| is not managed by the
// external filesystem provider.
bool ConvertFileToRelativeFileSystemPath(Profile* profile,
                                         const std::string& extension_id,
                                         const base::FilePath& full_file_path,
                                         base::FilePath* relative_path);

// Gets base file browser url for.
GURL GetFileBrowserUrlWithParams(
    ui::SelectFileDialog::Type type,
    const string16& title,
    const base::FilePath& default_virtual_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension);

// Get file dialog title string from its type.
string16 GetTitleFromType(ui::SelectFileDialog::Type type);

// Shows a freshly mounted removable drive.
// If there is another File Browser instance open this call does nothing.
// The mount event will cause file_manager.js to show the new drive in
// the left panel, and that is all we want.
// If there is no File Browser open, this call opens a new one pointing to
// |path|. In this case the tab will automatically close on |path| unmount.
void ViewRemovableDrive(const base::FilePath& path);

// Opens an action choice dialog for an external drive.
// One of the actions is opening the File Manager.
void OpenActionChoiceDialog(const base::FilePath& path);

// Opens item with the default File Browser handler.
void ViewItem(const base::FilePath& path);

// Opens file browser on the folder containing the file, with the file selected.
void ShowFileInFolder(const base::FilePath& path);

// Executes the built-in File Manager handler or tries to open |file| directly
// in the browser. Returns false if neither is possible.
bool ExecuteBuiltinHandler(
    Browser* browser,
    const base::FilePath& path,
    const std::string& internal_task_id);

bool ShouldBeOpenedWithPdfPlugin(Profile* profile, const char* file_extension);

// Converts the vector of progress status to their JSON (Value) form.
base::ListValue* ProgressStatusVectorToListValue(
    Profile* profile,
    const std::string& extension_id,
    const google_apis::OperationProgressStatusList& list);

}  // namespace file_manager_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_UTIL_H_
