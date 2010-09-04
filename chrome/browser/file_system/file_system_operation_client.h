// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CLIENT_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CLIENT_H_

#include <vector>

#include "base/file_util_proxy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

// Interface for client of FileSystemOperation.
class FileSystemOperationClient {
 public:
  virtual ~FileSystemOperationClient() {}

  virtual void DidFail(WebKit::WebFileError status, int request_id) = 0;

  virtual void DidSucceed(int request_id) = 0;

  // Info about the file entry such as modification date and size.
  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               int request_id) = 0;

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more,
      int request_id) = 0;
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CLIENT_H_

