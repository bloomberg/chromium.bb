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

#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"
#include "webkit/database/vfs_backend.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using webkit_database::VfsBackend;

const int kNumDeleteRetries = 3;
const int kDelayDeleteRetryMs = 100;

namespace {

struct OpenFileParams {
  FilePath db_dir;
  FilePath file_name;
  int desired_flags;
  base::ProcessHandle handle;
};

struct DeleteFileParams {
  FilePath db_dir;
  FilePath file_name;
  bool sync_dir;
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
  base::PlatformFile target_dir_handle = base::kInvalidPlatformFileValue;
  VfsBackend::OpenFile(params.file_name, params.db_dir, params.desired_flags,
                       params.handle, &target_handle, &target_dir_handle);

  ViewMsg_DatabaseOpenFileResponse_Params response_params;
#if defined(OS_WIN)
  response_params.file_handle = target_handle;
#elif defined(OS_POSIX)
  response_params.file_handle = base::FileDescriptor(target_handle, true);
  response_params.dir_handle = base::FileDescriptor(target_dir_handle, true);
#endif
  io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
          new ViewMsg_DatabaseOpenFileResponse(message_id, response_params)));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
static void DatabaseDeleteFile(MessageLoop* io_thread_message_loop,
                               const DeleteFileParams& params,
                               int32 message_id,
                               int reschedule_count,
                               ResourceMessageFilter* sender) {
  // Return an error if the file could not be deleted
  // after kNumDeleteRetries times.
  if (!reschedule_count) {
    io_thread_message_loop->PostTask(FROM_HERE,
        NewRunnableFunction(SendMessage, sender,
            new ViewMsg_DatabaseDeleteFileResponse(message_id,
                                                   SQLITE_IOERR_DELETE)));
    return;
  }

  int error_code =
      VfsBackend::DeleteFile(params.file_name, params.db_dir, params.sync_dir);
  if (error_code == SQLITE_IOERR_DELETE) {
    // If the file could not be deleted, try again.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableFunction(DatabaseDeleteFile, io_thread_message_loop,
                            params, message_id, reschedule_count - 1, sender),
        kDelayDeleteRetryMs);
    return;
  }

  io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
          new ViewMsg_DatabaseDeleteFileResponse(message_id, error_code)));
}

// Scheduled by the IO thread on the file thread.
// Gets the attributes of the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process.
static void DatabaseGetFileAttributes(MessageLoop* io_thread_message_loop,
                                      const FilePath& file_name,
                                      int32 message_id,
                                      ResourceMessageFilter* sender) {
  uint32 attributes = VfsBackend::GetFileAttributes(file_name);
  io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
          new ViewMsg_DatabaseGetFileAttributesResponse(
              message_id, attributes)));
}

// Scheduled by the IO thread on the file thread.
// Gets the size of the given file, then schedules a task
// on the IO thread's message loop to send an IPC back to
// the corresponding renderer process.
static void DatabaseGetFileSize(MessageLoop* io_thread_message_loop,
                                const FilePath& file_name,
                                int32 message_id,
                                ResourceMessageFilter* sender) {
  int64 size = VfsBackend::GetFileSize(file_name);
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


bool DatabaseDispatcherHost::IsDBMessage(const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_DatabaseOpenFile::ID:
    case ViewHostMsg_DatabaseDeleteFile::ID:
    case ViewHostMsg_DatabaseGetFileAttributes::ID:
    case ViewHostMsg_DatabaseGetFileSize::ID:
      return true;
    default:
      return false;
  }
}

bool DatabaseDispatcherHost::OnMessageReceived(
  const IPC::Message& message, bool* message_was_ok) {
  if (!IsDBMessage(message))
    return false;
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

void DatabaseDispatcherHost::OnDatabaseOpenFile(const FilePath& file_name,
                                                int desired_flags,
                                                int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);

  if (db_file_name.empty()) {
    ViewMsg_DatabaseOpenFileResponse_Params response_params;
#if defined(OS_WIN)
    response_params.file_handle = base::kInvalidPlatformFileValue;
#elif defined(OS_POSIX)
    response_params.file_handle =
        base::FileDescriptor(base::kInvalidPlatformFileValue, true);
    response_params.dir_handle =
        base::FileDescriptor(base::kInvalidPlatformFileValue, true);
#endif
    resource_message_filter_->Send(new ViewMsg_DatabaseOpenFileResponse(
        message_id, response_params));
    return;
  }

  OpenFileParams params;
  params.db_dir = GetDBDir();
  params.file_name = db_file_name;
  params.desired_flags = desired_flags;
  params.handle = resource_message_filter_->handle();
  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
      NewRunnableFunction(DatabaseOpenFile, MessageLoop::current(),
                          params, message_id, resource_message_filter_));
}

void DatabaseDispatcherHost::OnDatabaseDeleteFile(
  const FilePath& file_name, const bool& sync_dir, int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);
  if (db_file_name.empty()) {
    resource_message_filter_->Send(new ViewMsg_DatabaseDeleteFileResponse(
        message_id, SQLITE_IOERR_DELETE));
    return;
  }

  DeleteFileParams params;
  params.db_dir = GetDBDir();
  params.file_name = db_file_name;
  params.sync_dir = sync_dir;
  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
      NewRunnableFunction(DatabaseDeleteFile, MessageLoop::current(),
                          params, message_id, kNumDeleteRetries,
                          resource_message_filter_));
}

void DatabaseDispatcherHost::OnDatabaseGetFileAttributes(
  const FilePath& file_name, int32 message_id) {
  FilePath db_file_name = GetDBFileFullPath(file_name);
  if (db_file_name.empty()) {
    resource_message_filter_->Send(
        new ViewMsg_DatabaseGetFileAttributesResponse(message_id, -1));
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
