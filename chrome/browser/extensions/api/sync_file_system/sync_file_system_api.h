// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_

#include "base/platform_file.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class SyncFileSystemRequestFileSystemFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("syncFileSystem.requestFileSystem");

 protected:
  virtual ~SyncFileSystemRequestFileSystemFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void DidOpenFileSystem(base::PlatformFileError error,
                         const std::string& file_system_name,
                         const GURL& root_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
