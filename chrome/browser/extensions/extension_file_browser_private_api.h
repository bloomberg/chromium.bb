// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/extensions/extension_function.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

// Implements the Chrome Extension local File API.
class RequestLocalFileSystemFunction
    : public AsyncExtensionFunction {
 public:
  RequestLocalFileSystemFunction();

 protected:
  virtual ~RequestLocalFileSystemFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  friend class LocalFileSystemCallbackDispatcher;
  void RespondSuccessOnUIThread(const std::string& name,
                                const FilePath& path);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.requestLocalFileSystem");
};

// Parent class for the chromium extension APIs for the file dialog.
class FileDialogFunction
    : public AsyncExtensionFunction {
 public:
  typedef std::vector<FilePath> FilePathVec;

  FileDialogFunction() {}

 protected:
  virtual ~FileDialogFunction() {}

  // Convert virtual paths to local paths on the file thread.
  typedef std::vector<std::string> VirtualPathVec;
  void GetLocalPathsOnFileThread(const VirtualPathVec& virtual_paths);

  // Callback with converted local paths.
  virtual void GetLocalPathsResponseOnUIThread(const FilePathVec&
                                               local_paths) {}
};

// Select a single file.
class SelectFileFunction
    : public FileDialogFunction {
 public:
  SelectFileFunction() {}

  const FilePath& selected_file() const { return selected_file_; }
  int index() const { return index_; }

 protected:
  virtual ~SelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // FileDialogFunction overrides.
  virtual void GetLocalPathsResponseOnUIThread(const FilePathVec&
                                               local_paths) OVERRIDE;

 private:
  FilePath selected_file_;
  int index_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFile");
};

// Select multiple files.
class SelectFilesFunction
    : public FileDialogFunction {
 public:
  SelectFilesFunction() {}

  const FilePathVec& selected_files() const { return selected_files_; }

 protected:
  virtual ~SelectFilesFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // FileDialogFunction overrides.
  virtual void GetLocalPathsResponseOnUIThread(const FilePathVec&
                                               local_paths) OVERRIDE;

 private:
  FilePathVec selected_files_;

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFiles");
};

// Cancel file selection Dialog.
class CancelFileDialogFunction
    : public FileDialogFunction {
 public:
  CancelFileDialogFunction() {}

 protected:
  virtual ~CancelFileDialogFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.cancelDialog");
};

// File Dialog Strings.
class FileDialogStringsFunction
    : public FileDialogFunction {
 public:
  FileDialogStringsFunction() {}

 protected:
  virtual ~FileDialogStringsFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.getStrings");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
