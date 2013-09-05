// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides URL-related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_URL_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_URL_UTIL_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace file_manager {
namespace util {

// Returns the file manager's base URL, which just looks like
// chrome-extension://<app-id>/.
GURL GetFileManagerBaseUrl();

// Returns the file manager's main page URL.
GURL GetFileManagerMainPageUrl();

// Returns the file manager's main page URL with parameters encoded as JSON
// in the query string section. |file_types| is optional.
GURL GetFileManagerMainPageUrlWithParams(
    ui::SelectFileDialog::Type type,
    const string16& title,
    const base::FilePath& default_virtual_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension);

// Returns the file manager's action choice URL.
GURL GetActionChoiceUrl(const base::FilePath& virtual_path,
                        bool advanced_mode);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_URL_UTIL_H_
