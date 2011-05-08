// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
#pragma once

#include "base/file_path.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "googleurl/src/gurl.h"

class Profile;

// Helper class for wiring file browser component extension with the rest of UI.
class FileManagerUtil {
 public:
  // Gets base file browser url.
  static GURL GetFileBrowserUrl();
  // Converts |full_file_path| into external filesystem: url. Returns false
  // if |full_file_path| is not managed by the external filesystem provider.
  static bool ConvertFileToFileSystemUrl(Profile* profile,
      const FilePath& full_file_path, const GURL& origin_url, GURL* url);
  // Gets base file browser url for.
  static GURL GetFileBrowserUrlWithParams(
      SelectFileDialog::Type type,
      const string16& title,
      const FilePath& default_path,
      const SelectFileDialog::FileTypeInfo* file_types,
      int file_type_index,
      const FilePath::StringType& default_extension);

  // Opens file browser UI in its own tab on file system location defined with
  // |default_path|.
  static void ShowFullTabUrl(Profile* profile,
                             const FilePath& default_path);

  static void ViewItem(const FilePath& full_path, bool enqueue);

 private:
  FileManagerUtil() {}
  // Helper to convert numeric dialog type to a string.
  static std::string GetDialogTypeAsString(SelectFileDialog::Type dialog_type);
  // Help to convert potential dialog arguments into json.
  static std::string GetArgumentsJson(
      SelectFileDialog::Type type,
      const string16& title,
      const FilePath& default_path,
      const SelectFileDialog::FileTypeInfo* file_types,
      int file_type_index,
      const FilePath::StringType& default_extension);

  DISALLOW_COPY_AND_ASSIGN(FileManagerUtil);
};

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_MANAGER_UTIL_H_
