// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
#pragma once

#include <string>

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

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FILE_BROWSER_PRIVATE_API_H_
