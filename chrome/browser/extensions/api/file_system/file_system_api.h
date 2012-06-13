// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class FileSystemGetDisplayPathFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getDisplayPath");

 protected:
  virtual ~FileSystemGetDisplayPathFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FileSystemGetWritableFileEntryFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("fileSystem.getWritableFileEntry");

 protected:
  virtual ~FileSystemGetWritableFileEntryFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  class FilePicker;

  void FileSelected(const FilePath& path);
  void FileSelectionCanceled();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
