// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides functions for opening an item (file or directory) using
// the file manager.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_UTIL_H_

namespace base {
class FilePath;
}

namespace file_manager {
namespace util {

// Opens the file manager for the freshly mounted removable drive specified
// by |file_path|.
// If there is another file manager instance open, this call does nothing.
// The mount event will cause the file manager to show the new drive in
// the left panel.
// If there is no file manager open, this call opens a new one pointing to
// |file_path|. In this case the tab will automatically close on |file_path|
// unmount.
void OpenRemovableDrive(const base::FilePath& file_path);

// Opens an item (file or directory). If the target is a directory, the
// directory will be opened in the file manager. If the target is a file, the
// file will be opened using a file handler, a file browser handler, or the
// browser (open in a tab). The default handler has precedence over other
// handlers, if defined for the type of the target file.
void OpenItem(const base::FilePath& file_path);

// Opens the file manager for the folder containing the item specified by
// |file_path|, with the item selected.
void ShowItemInFolder(const base::FilePath& file_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_UTIL_H_
