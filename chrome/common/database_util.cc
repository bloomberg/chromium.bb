// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/database_util.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebKitClient;
using WebKit::WebString;

WebKitClient::FileHandle DatabaseUtil::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags,
    WebKitClient::FileHandle* dir_handle) {
  IPC::PlatformFileForTransit file_handle;
#if defined(OS_WIN)
  file_handle = base::kInvalidPlatformFileValue;
#elif defined(OS_POSIX)
  file_handle =
      base::FileDescriptor(base::kInvalidPlatformFileValue, true);
  base::FileDescriptor dir_handle_rv =
      base::FileDescriptor(base::kInvalidPlatformFileValue, true);
#endif

  scoped_refptr<IPC::SyncMessageFilter> filter =
      ChildThread::current()->sync_message_filter();

  filter->Send(new ViewHostMsg_DatabaseOpenFile(
      vfs_file_name,
      desired_flags,
      &file_handle
#if defined(OS_POSIX)
      , &dir_handle_rv
#endif
      ));

#if defined(OS_WIN)
  return file_handle;
#elif defined(OS_POSIX)
  if (dir_handle)
    *dir_handle = dir_handle_rv.fd;
  return file_handle.fd;
#endif
}

int DatabaseUtil::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  int rv = SQLITE_IOERR_DELETE;
  scoped_refptr<IPC::SyncMessageFilter> filter =
      ChildThread::current()->sync_message_filter();
  filter->Send(new ViewHostMsg_DatabaseDeleteFile(
      vfs_file_name, sync_dir, &rv));
  return rv;
}

long DatabaseUtil::databaseGetFileAttributes(const WebString& vfs_file_name) {
  int32 rv = -1;
  scoped_refptr<IPC::SyncMessageFilter> filter =
      ChildThread::current()->sync_message_filter();
  filter->Send(new ViewHostMsg_DatabaseGetFileAttributes(vfs_file_name, &rv));
  return rv;
}

long long DatabaseUtil::databaseGetFileSize(const WebString& vfs_file_name) {
  int64 rv = 0LL;
  scoped_refptr<IPC::SyncMessageFilter> filter =
      ChildThread::current()->sync_message_filter();
  filter->Send(new ViewHostMsg_DatabaseGetFileSize(vfs_file_name, &rv));
  return rv;
}
