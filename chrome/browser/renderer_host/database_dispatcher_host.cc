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

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

const int kNumDeleteRetries = 5;
const int kDelayDeleteRetryMs = 100;

namespace {

struct OpenFileParams {
  FilePath db_dir; // directory where all DB files are stored
  FilePath file_name; // DB file
  int desired_flags;  // flags to be used to open the file
  base::ProcessHandle handle; // the handle of the renderer process
};

struct DeleteFileParams {
  FilePath db_dir; // directory where all DB files are stored
  FilePath file_name; // DB file
  bool sync_dir; // sync DB directory after the file is deleted?
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

// Make sure the flags used to open a DB file are consistent.
static bool OpenFileFlagsAreConsistent(const OpenFileParams& params) {
  if (params.file_name == params.db_dir) {
    return (params.desired_flags == SQLITE_OPEN_READONLY);
  }

  const int file_type = params.desired_flags & 0x00007F00;
  const bool is_exclusive =
    (params.desired_flags & SQLITE_OPEN_EXCLUSIVE) != 0;
  const bool is_delete =
    (params.desired_flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
  const bool is_create =
    (params.desired_flags & SQLITE_OPEN_CREATE) != 0;
  const bool is_read_only =
    (params.desired_flags & SQLITE_OPEN_READONLY) != 0;
  const bool is_read_write =
    (params.desired_flags & SQLITE_OPEN_READWRITE) != 0;

  // All files should be opened either read-write or read-only.
  if (!(is_read_only ^ is_read_write)) {
    return false;
  }

  // If a new file is created, it must also be writtable.
  if (is_create && !is_read_write) {
    return false;
  }

  // We must be able to create a new file, if exclusive access is desired.
  if (is_exclusive && !is_create) {
    return false;
  }

  // We cannot delete the files that we expect to already exist.
  if (is_delete && !is_create) {
    return false;
  }

  // The main DB, main journal and master journal cannot be auto-deleted.
  if (((file_type == SQLITE_OPEN_MAIN_DB) ||
       (file_type == SQLITE_OPEN_MAIN_JOURNAL) ||
       (file_type == SQLITE_OPEN_MASTER_JOURNAL)) &&
      is_delete) {
    return false;
  }

  // Make sure we're opening the DB directory or that a file type is set.
  if ((file_type != SQLITE_OPEN_MAIN_DB) &&
      (file_type != SQLITE_OPEN_TEMP_DB) &&
      (file_type != SQLITE_OPEN_MAIN_JOURNAL) &&
      (file_type != SQLITE_OPEN_TEMP_JOURNAL) &&
      (file_type != SQLITE_OPEN_SUBJOURNAL) &&
      (file_type != SQLITE_OPEN_MASTER_JOURNAL) &&
      (file_type != SQLITE_OPEN_TRANSIENT_DB)) {
    return false;
  }

  return true;
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
#if defined(OS_POSIX)
  base::PlatformFile target_dir_handle = base::kInvalidPlatformFileValue;
#endif

  // Verify the flags for consistency and create the database
  // directory if it doesn't exist.
  if (OpenFileFlagsAreConsistent(params) &&
      file_util::CreateDirectory(params.db_dir)) {
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

    if (params.desired_flags & SQLITE_OPEN_EXCLUSIVE) {
      flags |= base::PLATFORM_FILE_EXCLUSIVE_READ |
               base::PLATFORM_FILE_EXCLUSIVE_WRITE;
    }

    if (params.desired_flags & SQLITE_OPEN_DELETEONCLOSE) {
      flags |= base::PLATFORM_FILE_TEMPORARY | base::PLATFORM_FILE_HIDDEN |
               base::PLATFORM_FILE_DELETE_ON_CLOSE;
    }

    // Try to open/create the DB file.
    base::PlatformFile file_handle =
      base::CreatePlatformFile(params.file_name.ToWStringHack(), flags, NULL);
    if (file_handle != base::kInvalidPlatformFileValue) {
#if defined(OS_WIN)
      // Duplicate the file handle.
      if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                           params.handle, &target_handle, 0, false,
                           DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
          // file_handle is closed whether or not DuplicateHandle succeeds.
          target_handle = INVALID_HANDLE_VALUE;
      }
#elif defined(OS_POSIX)
      target_handle = file_handle;

      int file_type = params.desired_flags & 0x00007F00;
      bool creating_new_file = (params.desired_flags & SQLITE_OPEN_CREATE);
      if (creating_new_file && ((file_type == SQLITE_OPEN_MASTER_JOURNAL) ||
                                (file_type == SQLITE_OPEN_MAIN_JOURNAL))) {
        // We return a handle to the containing directory because on POSIX
        // systems the VFS might want to fsync it after changing a file.
        // By returning it here, we avoid an extra IPC call.
        target_dir_handle = base::CreatePlatformFile(
            params.db_dir.ToWStringHack(),
            base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL);
        if (target_dir_handle == base::kInvalidPlatformFileValue) {
          base::ClosePlatformFile(target_handle);
          target_handle = base::kInvalidPlatformFileValue;
        }
      }
#endif
    }
  }

  ViewMsg_DatabaseOpenFileResponse_Params response_params =
#if defined(OS_WIN)
    { target_handle };
#elif defined(OS_POSIX)
    { base::FileDescriptor(target_handle, true),
      base::FileDescriptor(target_dir_handle, true) };
#endif

   io_thread_message_loop->PostTask(FROM_HERE,
     NewRunnableFunction(SendMessage, sender,
       new ViewMsg_DatabaseOpenFileResponse(message_id, response_params)));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
static void DatabaseDeleteFile(
    MessageLoop* io_thread_message_loop,
    const DeleteFileParams& params,
    int32 message_id,
    int reschedule_count,
    ResourceMessageFilter* sender) {
  // Return an error if the file could not be deleted
  // after kNumDeleteRetries times.
  if (!reschedule_count) {
    io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
        new ViewMsg_DatabaseDeleteFileResponse(
          message_id, SQLITE_IOERR_DELETE)));
    return;
  }

  // If the file does not exist, we're done.
  if (!file_util::PathExists(params.file_name)) {
    io_thread_message_loop->PostTask(FROM_HERE,
      NewRunnableFunction(SendMessage, sender,
        new ViewMsg_DatabaseDeleteFileResponse(message_id, SQLITE_OK)));
    return;
  }


  // If the file could not be deleted, try again.
  if (!file_util::Delete(params.file_name, false)) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(DatabaseDeleteFile, io_thread_message_loop,
        params, message_id, reschedule_count - 1, sender),
      kDelayDeleteRetryMs);
    return;
  }

  // File existed and it was successfully deleted
  int error_code = SQLITE_OK;
#if defined(OS_POSIX)
  // sync the DB directory if needed
  if (params.sync_dir) {
    base::PlatformFile dir_fd = base::CreatePlatformFile(
      params.db_dir.ToWStringHack(), base::PLATFORM_FILE_READ, NULL);
    if (dir_fd == base::kInvalidPlatformFileValue) {
      error_code = SQLITE_CANTOPEN;
    } else {
      if (fsync(dir_fd)) {
        error_code = SQLITE_IOERR_DIR_FSYNC;
      }
      base::ClosePlatformFile(dir_fd);
    }
  }
#endif

  io_thread_message_loop->PostTask(FROM_HERE,
    NewRunnableFunction(SendMessage, sender,
      new ViewMsg_DatabaseDeleteFileResponse(message_id, error_code)));
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
#elif defined(OS_POSIX)
  uint32 attributes = 0;
  if (!access(file_name.value().c_str(), R_OK)) {
    attributes |= static_cast<uint32>(R_OK);
  }
  if (!access(file_name.value().c_str(), W_OK)) {
    attributes |= static_cast<uint32>(W_OK);
  }
  if (!attributes) {
    attributes = -1;
  }
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
  FilePath db_file_name = GetDBFileFullPath(file_name);

  if (db_file_name.empty()) {
    ViewMsg_DatabaseOpenFileResponse_Params response_params =
#if defined(OS_WIN)
      { base::kInvalidPlatformFileValue };
#elif defined(OS_POSIX)
      { base::FileDescriptor(base::kInvalidPlatformFileValue, true),
        base::FileDescriptor(base::kInvalidPlatformFileValue, true) };
#endif
    resource_message_filter_->Send(new ViewMsg_DatabaseOpenFileResponse(
      message_id, response_params));
    return;
  }

  OpenFileParams params = { GetDBDir(), db_file_name, desired_flags,
    resource_message_filter_->handle() };
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

  DeleteFileParams params = { GetDBDir(), db_file_name, sync_dir };
  resource_message_filter_->AddRef();
  file_thread_message_loop_->PostTask(FROM_HERE,
    NewRunnableFunction(DatabaseDeleteFile, MessageLoop::current(),
      params, message_id, kNumDeleteRetries, resource_message_filter_));
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
