// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DATABASE_UTIL_H_
#define CHROME_COMMON_DATABASE_UTIL_H_

#include "webkit/glue/webkitclient_impl.h"

// A class of utility functions used by RendererWebKitClientImpl and
// WorkerWebKitClientImpl to handle database file accesses.
class DatabaseUtil {
 public:
  static WebKit::WebKitClient::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  static int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                bool sync_dir);
  static long databaseGetFileAttributes(const WebKit::WebString& vfs_file_name);
  static long long databaseGetFileSize(const WebKit::WebString& vfs_file_name);
};

#endif  // CHROME_COMMON_DATABASE_UTIL_H_
