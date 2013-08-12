// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Browser;
class GURL;
class Profile;

extern const char kFileBrowserDomain[];
extern const char kFileBrowserGalleryTaskId[];
extern const char kFileBrowserWatchTaskId[];

// File manager helper methods.
namespace file_manager {
namespace util {

// Gets base file browser url.
GURL GetFileBrowserExtensionUrl();
GURL GetFileBrowserUrl();
GURL GetMediaPlayerUrl();

// Converts |relative_path| (e.g., "drive/root" or "Downloads") into external
// filesystem URL (e.g., filesystem://id/external/drive/root).
GURL ConvertRelativePathToFileSystemUrl(const base::FilePath& relative_path,
                                        const std::string& extension_id);

// Converts |full_file_path| (e.g., "/special/drive/root" or
// "/home/chronos/user/Downloads") into external filesystem URL. Returns false
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
// One of the actions is opening the File Manager. Passes |advanced_mode|
// flag to the dialog. If it is enabled, then auto-choice gets disabled.
void OpenActionChoiceDialog(const base::FilePath& path, bool advanced_mode);

// Opens an item (file or directory). If the target is a directory, the
// directory will be opened in the file manager. If the target is a file, the
// file will be opened using a file handler, a file browser handler, or the
// browser (open in a tab). The default handler has precedence over other
// handlers, if defined for the type of the target file.
void ViewItem(const base::FilePath& path);

// Opens file browser on the folder containing the file, with the file selected.
void ShowFileInFolder(const base::FilePath& path);

// Opens the file specified by |path| with the browser. This function takes
// care of the following intricacies:
//
// - If the file is a Drive hosted document, the hosted document will be
//   opened in the browser by extracting the right URL for the file.
// - If the file is a CRX file, the CRX file will be installed.
// - If the file is on Drive, the file will be downloaded from Drive as
//   needed.
//
// Returns false if failed to open. This happens if the file type is unknown.
bool OpenFileWithBrowser(Browser* browser, const base::FilePath& path);

// Checks whether a pepper plugin for |file_extension| is enabled.
bool ShouldBeOpenedWithPlugin(Profile* profile, const char* file_extension);

// Returns the MIME type of |file_path|.
std::string GetMimeTypeForPath(const base::FilePath& file_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_UTIL_H_
