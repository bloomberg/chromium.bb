// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "googleurl/src/gurl.h"

class Profile;

extern const char kFileBrowserDomain[];

// File manager helper methods.
namespace file_manager_util {

// Gets base file browser url.
GURL GetFileBrowserExtensionUrl();
GURL GetFileBrowserUrl();
GURL GetMediaPlayerUrl();
GURL GetMediaPlayerPlaylistUrl();

// Converts |full_file_path| into external filesystem: url. Returns false
// if |full_file_path| is not managed by the external filesystem provider.
bool ConvertFileToFileSystemUrl(Profile* profile,
    const FilePath& full_file_path, const GURL& origin_url, GURL* url);

// Converts |full_file_path| into |relative_path| within the external provider
// in File API. Returns false if |full_file_path| is not managed by the
// external filesystem provider.
bool ConvertFileToRelativeFileSystemPath(Profile* profile,
    const FilePath& full_file_path, FilePath* relative_path);

// Gets base file browser url for.
GURL GetFileBrowserUrlWithParams(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_virtual_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension);

// Opens file browser UI in its own tab on file system location defined with
// |dir|.
void ViewFolder(const FilePath& dir);

void ViewItem(const FilePath& full_path, bool enqueue);

}  // namespace file_manager_util

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
