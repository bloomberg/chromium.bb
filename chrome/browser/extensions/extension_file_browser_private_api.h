// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/platform_file.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

class GURL;

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
                                const GURL& root);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);

  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.requestLocalFileSystem");
};

// Parent class for the chromium extension APIs for the file dialog.
class FileDialogFunction
    : public AsyncExtensionFunction {
 public:
  typedef std::vector<std::string> VirtualPathVec;
  typedef std::vector<FilePath> FilePathVec;

  FileDialogFunction();

  // Methods to register/unregister <tab_id, listener> tuples.
  // When file selection events occur in a tab, we'll call back the
  // appropriate listener.
  static void AddListener(int32 tab_id, SelectFileDialog::Listener* l);
  static void RemoveListener(int32 tab_id);

 protected:
  virtual ~FileDialogFunction();

  // Convert virtual paths to local paths on the file thread.
  void GetLocalPathsOnFileThread();

  // Callback with converted local paths.
  virtual void GetLocalPathsResponseOnUIThread() {}

  // Get the listener for the hosting tab.
  SelectFileDialog::Listener* GetListener() const;

  VirtualPathVec virtual_paths_;
  FilePathVec selected_files_;

 private:
  // Figure out the tab_id of the hosting tab.
  int32 GetTabId() const;

  typedef std::map<int32, SelectFileDialog::Listener*> ListenerMap;
  static ListenerMap listener_map_;
};

// Select a single file.
class SelectFileFunction
    : public FileDialogFunction {
 public:
  SelectFileFunction() {}

 protected:
  virtual ~SelectFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // FileDialogFunction overrides.
  virtual void GetLocalPathsResponseOnUIThread() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("fileBrowserPrivate.selectFile");
};

// Select multiple files.
class SelectFilesFunction
    : public FileDialogFunction {
 public:
  SelectFilesFunction();

 protected:
  virtual ~SelectFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // FileDialogFunction overrides.
  virtual void GetLocalPathsResponseOnUIThread() OVERRIDE;

 private:
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
