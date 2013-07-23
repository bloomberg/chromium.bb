// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DATABASE_UTIL_H_
#define CONTENT_CHILD_DATABASE_UTIL_H_

#include "webkit/child/webkitplatformsupport_impl.h"

namespace IPC {
class SyncMessageFilter;
}

namespace content {
// A class of utility functions used by RendererWebKitPlatformSupportImpl and
// WorkerWebKitPlatformSupportImpl to handle database file accesses.
class DatabaseUtil {
 public:
  static WebKit::Platform::FileHandle DatabaseOpenFile(
      const WebKit::WebString& vfs_file_name,
      int desired_flags,
      IPC::SyncMessageFilter* sync_message_filter);
  static int DatabaseDeleteFile(
      const WebKit::WebString& vfs_file_name,
      bool sync_dir,
      IPC::SyncMessageFilter* sync_message_filter);
  static long DatabaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name,
      IPC::SyncMessageFilter* sync_message_filter);
  static long long DatabaseGetFileSize(
      const WebKit::WebString& vfs_file_name,
      IPC::SyncMessageFilter* sync_message_filter);
  static long long DatabaseGetSpaceAvailable(
      const WebKit::WebString& origin_identifier,
      IPC::SyncMessageFilter* sync_message_filter);
};

}  // namespace content

#endif  // CONTENT_CHILD_DATABASE_UTIL_H_
