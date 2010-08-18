// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/database_util.h"

#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebKitClient;
using WebKit::WebString;

WebKitClient::FileHandle DatabaseUtil::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  IPC::PlatformFileForTransit file_handle =
      IPC::InvalidPlatformFileForTransit();

  scoped_refptr<IPC::SyncMessageFilter> filter =
      ChildThread::current()->sync_message_filter();
  filter->Send(new ViewHostMsg_DatabaseOpenFile(
      vfs_file_name, desired_flags, &file_handle));

  return IPC::PlatformFileForTransitToPlatformFile(file_handle);
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
