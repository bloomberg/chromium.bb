// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/database_util.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "chrome/common/db_message_filter.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebKitClient;
using WebKit::WebString;

WebKitClient::FileHandle DatabaseUtil::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags,
    WebKitClient::FileHandle* dir_handle) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();

#if defined(OS_WIN)
  ViewMsg_DatabaseOpenFileResponse_Params default_response =
      { base::kInvalidPlatformFileValue };
#elif defined(OS_POSIX)
  ViewMsg_DatabaseOpenFileResponse_Params default_response =
      { base::FileDescriptor(base::kInvalidPlatformFileValue, true),
        base::FileDescriptor(base::kInvalidPlatformFileValue, true) };
#endif

  ViewMsg_DatabaseOpenFileResponse_Params result =
      db_message_filter->SendAndWait(
          new ViewHostMsg_DatabaseOpenFile(
              vfs_file_name, desired_flags, message_id),
          message_id, default_response);

#if defined(OS_WIN)
  if (dir_handle)
    *dir_handle = base::kInvalidPlatformFileValue;
  return result.file_handle;
#elif defined(OS_POSIX)
  if (dir_handle)
    *dir_handle = result.dir_handle.fd;
  return result.file_handle.fd;
#endif
}

int DatabaseUtil::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
      new ViewHostMsg_DatabaseDeleteFile(vfs_file_name, sync_dir, message_id),
      message_id, SQLITE_IOERR_DELETE);
}

long DatabaseUtil::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
      new ViewHostMsg_DatabaseGetFileAttributes(vfs_file_name, message_id),
      message_id, -1L);
}

long long DatabaseUtil::databaseGetFileSize(
    const WebString& vfs_file_name) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
      new ViewHostMsg_DatabaseGetFileSize(vfs_file_name, message_id),
      message_id, 0LL);
}
