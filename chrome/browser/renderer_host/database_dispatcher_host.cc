// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_dispatcher_host.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_message.h"

const int kNumDeleteRetries = 5;
const int kDelayDeleteRetryMs = 100;

namespace {

struct OpenFileParams {
  FilePath db_dir; // directory where all DB files are stored
  FilePath file_name; // DB file
  int desired_flags;  // flags to be used to open the file
  base::ProcessHandle handle; // the handle of the renderer process
};

// Scheduled by the file Thread on the IO thread.
// Sends back to the renderer process the given message.
static void SendMessage(ResourceMessageFilter* sender,
                        IPC::Message* message) {
  sender->Send(message);

  // Every time we get a DB-related message, we AddRef() the resource
  // message filterto make sure it doesn't get destroyed before we have
  // a chance to send the reply back. So we need to Release() is here
  // and allow it to be destroyed if needed.
  sender->Release();
}

// Scheduled by the IO thread on the file thread.
// Opens the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the file handle.
static void DatabaseOpenFile(MessageLoop* io_thread_message_loop,
                             const OpenFileParams& params,
                             int32 message_id,
                             ResourceMessageFilter* sender) {
  base::PlatformFile target_handle = base::kInvalidPlatformFileValue;
  // Create the database directory if it doesn't exist.
  if (file_util::CreateDirectory(params.db_dir)) {
    int flags = 0;
    flags |= base::PLATFORM_FILE_READ;
    if (params.desired_flags & SQLITE_OPEN_READWRITE) {
      flags |= base::PLATFORM_FILE_WRITE;
    }

    if (!(params.desired_flags & SQLITE_OPEN_MAIN_DB)) {
      flags |= base::PLATFORM_FILE_EXCLUSIVE_READ |
               base::PLATFORM_FILE_EXCLUSIVE_WRITE;
    }

    if (params.desired_flags & SQLITE_OPEN_CREATE) {
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
    } else {
      flags |= base::PLATFORM_FILE_OPEN;
    }

    if (params.desired_flags & SQLITE_OPEN_DELETEONCLOSE) {
      flags |= base::PLATFORM_FILE_TEMPORARY | base::PLATFORM_FILE_HIDDEN |
               base::PLATFORM_FILE_DELETE_ON_CLOSE;
    }

    // Try to open/create the DB file.
#if defined(OS_WIN)
    base::PlatformFile file_handle =
      base::CreatePlatformFile(params.file_name.value(), flags, NULL);
    if (file_handle != base::kInvalidPlatformFileValue) {
      // Duplicate the file handle.
      if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                           params.handle, &target_handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
          // file_handle is closed whether or not DuplicateHandle succeeds.
          target_handle = INVALID_HANDLE_VALUE;
      }
    }
#endif
  }

  io_thread_message_loop->PostTask(FROM_HERE,
    NewRunnableFunction(SendMessage, sender,
      new ViewMsg_DatabaseOpenFileResponse(message_id, target_handle)));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
static void DatabaseDeleteFile(
    MessageLoop* io_thread_message_loop,
    const FilePath& file_name,
    int32 message_id,
    int reschedule_count,
    ResourceMessageFilter* sender) {
  if (reschedule_count > 0 && file_util::PathExists(file_name) &&
      !file_util::Delete(file_name, false)) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(DatabaseDeleteFile, io_thread_message_loop,
        file_name, message_id, reschedule_count - 1, sender),
      kDelayDeleteRetryMs);
  } else {
    io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
        new ViewMsg_DatabaseDeleteFileResponse(
          message_id, reschedule_count > 0)));
  }
}

// Scheduled by the IO thread on the file thread.
// Gets the attributes of the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process.
static void DatabaseGetFileAttributes(
    MessageLoop* io_thread_message_loop,
    const FilePath& file_name,
    int32 message_id,
    ResourceMessageFilter* sender) {
#if defined(OS_WIN)
  uint32 attributes = GetFileAttributes(file_name.value().c_str());
#else
  uint32 attributes = -1L;
#endif

  io_thread_message_loop->PostTask(FROM_HERE,
    NewRunnableFunction(SendMessage, sender,
      new ViewMsg_DatabaseGetFileAttributesResponse(message_id, attributes)));
}

// Scheduled by the IO thread on the file thread.
// Gets the size of the given file, then schedules a task
// on the IO thread's message loop to send an IPC back to
// the corresponding renderer process.
static void DatabaseGetFileSize(
    MessageLoop* io_thread_message_loop,
    const FilePath& file_name,
    int32 message_id,
    ResourceMessageFilter* sender) {
  int64 size = 0;
  if (!file_util::GetFileSize(file_name, &size)) {
    size = 0;
  }

  io_thread_message_loop->PostTask(FROM_HERE,
    NewRunnableFunction(SendMessage, sender,
      new ViewMsg_DatabaseGetFileSizeResponse(message_id, size)));
}

} // namespace

DatabaseDispatcherHost::DatabaseDispatcherHost(
  const FilePath& profile_path,
  ResourceMessageFilter* resource_message_filter)
  : profile_path_(profile_path),
    resource_message_filter_(resource_message_filter),
    file_thread_message_loop_(
      g_browser_process->file_thread()->message_loop()) {
}

DatabaseDispatcherHost::~DatabaseDispatcherHost() {
}

bool DatabaseDispatcherHost::IsDBMessage(const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_DatabaseOpenFile::ID:
    case ViewHostMsg_DatabaseDeleteFile::ID:
    case ViewHostMsg_DatabaseGetFileAttributes::ID:
    case ViewHostMsg_DatabaseGetFileSize::ID:
      return true;
  }
  return false;
}

bool DatabaseDispatcherHost::OnMessageReceived(
  const IPC::Message& message, bool* message_was_ok) {
  if (!IsDBMessage(message)) {
    return false;
  }
  *message_was_ok = true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DatabaseDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpenFile, OnDatabaseOpenFile);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseDeleteFile, OnDatabaseDeleteFile);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileAttributes,
                        OnDatabaseGetFileAttributes);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileSize,
                        OnDatabaseGetFileSize);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

FilePath DatabaseDispatcherHost::GetDBDir() {
  return profile_path_.Append(FILE_PATH_LITERAL("databases"));
}

FilePath DatabaseDispatcherHost::GetDBFileFullPath(const FilePath& file_name) {
  // Do not allow '\', '/' and ':' in file names.
  FilePath::StringType file = file_name.value();
  if ((file.find('\\') != std::wstring::npos) ||
      (file.find('/') != std::wstring::npos) ||
      (file.find(':') != std::wstring::npos)) {
    return FilePath();
  }
  return GetDBDir().Append(file_name);
}

void DatabaseDispatcherHost::OnDatabaseOpenFile(
  const FilePath& file_name, int desired_flags,
  int32 message_id) {
  FilePath db_dir = GetDBDir();
  FilePath db_file_name = GetDBFileFullPath(file_name);

  if (db_file_name.empty()) {
    resource_message_filter_->Send(new ViewMsg_DatabaseOpenFileResponse(
      message_id, base::kInvalidPlatformFileValue));
    return;
  }

  OpenFileParams params = { db_dir, db_file_name, desired_flags,
    resource_message_filter_->handle() };
  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(DatabaseOpenFile, MessageLoop::current(),
      params, message_id, resource_message_filter_));
}

void DatabaseDispatcherHost::OnDatabaseDeleteFile(
  const FilePath& file_name, int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);
  if (db_file_name.empty()) {
    resource_message_filter_->Send(new ViewMsg_DatabaseDeleteFileResponse(
      message_id, false));
    return;
  }

  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(DatabaseDeleteFile, MessageLoop::current(),
      db_file_name, message_id, kNumDeleteRetries, resource_message_filter_));
}

void DatabaseDispatcherHost::OnDatabaseGetFileAttributes(
  const FilePath& file_name, int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);
  if (db_file_name.empty()) {
    resource_message_filter_->Send(
      new ViewMsg_DatabaseGetFileAttributesResponse(
        message_id, -1));
    return;
  }

  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(DatabaseGetFileAttributes, MessageLoop::current(),
      db_file_name, message_id, resource_message_filter_));
}

void DatabaseDispatcherHost::OnDatabaseGetFileSize(
  const FilePath& file_name, int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);
  if (db_file_name.empty()) {
    resource_message_filter_->Send(new ViewMsg_DatabaseGetFileSizeResponse(
      message_id, 0));
    return;
  }

  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(DatabaseGetFileSize, MessageLoop::current(),
      db_file_name, message_id, resource_message_filter_));
}
