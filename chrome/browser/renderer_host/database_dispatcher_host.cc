// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_dispatcher_host.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/database_permission_request.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/database/database_util.h"
#include "webkit/database/vfs_backend.h"

using WebKit::WebSecurityOrigin;
using webkit_database::DatabaseTracker;
using webkit_database::DatabaseUtil;
using webkit_database::VfsBackend;

const int kNumDeleteRetries = 2;
const int kDelayDeleteRetryMs = 100;

DatabaseDispatcherHost::DatabaseDispatcherHost(
    DatabaseTracker* db_tracker,
    ResourceMessageFilter* resource_message_filter)
    : db_tracker_(db_tracker),
      resource_message_filter_(resource_message_filter),
      process_handle_(0),
      observer_added_(false),
      shutdown_(false) {
  DCHECK(db_tracker_);
  DCHECK(resource_message_filter_);
}

void DatabaseDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
}

void DatabaseDispatcherHost::Shutdown() {
  shutdown_ = true;
  resource_message_filter_ = NULL;
  if (observer_added_) {
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::RemoveObserver));
  }
}

void DatabaseDispatcherHost::AddObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  db_tracker_->AddObserver(this);
}

void DatabaseDispatcherHost::RemoveObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // If the renderer process died without closing all databases,
  // then we need to manually close those connections
  db_tracker_->CloseDatabases(database_connections_);
  database_connections_.RemoveAllConnections();

  db_tracker_->RemoveObserver(this);
}

bool DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(!shutdown_);
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DatabaseDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpenFile, OnDatabaseOpenFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseDeleteFile, OnDatabaseDeleteFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileAttributes,
                        OnDatabaseGetFileAttributes)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseGetFileSize,
                        OnDatabaseGetFileSize)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpened, OnDatabaseOpened)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseModified, OnDatabaseModified)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseClosed, OnDatabaseClosed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DatabaseDispatcherHost::ReceivedBadMessage(uint32 msg_type) {
  BrowserRenderProcessHost::BadMessageTerminateProcess(
      msg_type, process_handle_);
}

// Scheduled by the file thread on the IO thread.
// Sends back to the renderer process the given message.
void DatabaseDispatcherHost::SendMessage(IPC::Message* message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!shutdown_)
    resource_message_filter_->Send(message);
  else
    delete message;
}

void DatabaseDispatcherHost::OnDatabaseOpenFile(const string16& vfs_file_name,
                                                int desired_flags,
                                                int32 message_id) {
  if (!observer_added_) {
    observer_added_ = true;
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::AddObserver));
  }

  // Only ask permission on the main database file in read/write mode.
  if (!VfsBackend::FileTypeIsMainDB(desired_flags) ||
      !VfsBackend::OpenTypeIsReadWrite(desired_flags)) {
    OnDatabaseOpenFileAllowed(vfs_file_name, desired_flags, message_id);
    return;
  }

  string16 origin_identifier;
  string16 database_name;
  bool ok = DatabaseUtil::CrackVfsFileName(vfs_file_name,
                                           &origin_identifier,
                                           &database_name,
                                           NULL);
  DCHECK(ok);  // Should we assume this is an attack and kill the renderer?
  if (!ok) {
    OnDatabaseOpenFileBlocked(message_id);
    return;
  }

  // TODO(jorlow): createFromDatabaseIdentifier should not return a pointer.
  scoped_ptr<WebSecurityOrigin> security_origin(
      WebSecurityOrigin::createFromDatabaseIdentifier(origin_identifier));
  string16 origin(security_origin->toString());
  GURL url = GURL(origin);

  HostContentSettingsMap* host_content_settings_map = resource_message_filter_->
      GetRequestContextForURL(url)->host_content_settings_map();
  ContentSetting content_setting = host_content_settings_map->GetContentSetting(
      url.host(), CONTENT_SETTINGS_TYPE_COOKIES);

  if (content_setting == CONTENT_SETTING_ASK) {
    // Create a task for each possible outcome.
    scoped_ptr<Task> on_allow(NewRunnableMethod(
        this, &DatabaseDispatcherHost::OnDatabaseOpenFileAllowed,
        vfs_file_name, desired_flags, message_id));
    scoped_ptr<Task> on_block(NewRunnableMethod(
        this, &DatabaseDispatcherHost::OnDatabaseOpenFileBlocked, message_id));
    // And then let the permission request object do the rest.
    scoped_refptr<DatabasePermissionRequest> request(
        new DatabasePermissionRequest(url, database_name, on_allow.release(),
                                      on_block.release(),
                                      host_content_settings_map));
    request->RequestPermission();
  } else if (content_setting == CONTENT_SETTING_ALLOW) {
    OnDatabaseOpenFileAllowed(vfs_file_name, desired_flags, message_id);
  } else {
    DCHECK(content_setting == CONTENT_SETTING_BLOCK);
    OnDatabaseOpenFileBlocked(message_id);
  }
}

static void SetOpenFileResponseParams(
    ViewMsg_DatabaseOpenFileResponse_Params* params,
    base::PlatformFile file_handle,
    base::PlatformFile dir_handle) {
#if defined(OS_WIN)
  params->file_handle = file_handle;
#elif defined(OS_POSIX)
  params->file_handle = base::FileDescriptor(file_handle, true);
  params->dir_handle = base::FileDescriptor(dir_handle, true);
#endif
}

// Scheduled by the IO thread on the file thread.
// Opens the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the file handle.
void DatabaseDispatcherHost::DatabaseOpenFile(const string16& vfs_file_name,
                                              int desired_flags,
                                              int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  base::PlatformFile target_handle = base::kInvalidPlatformFileValue;
  base::PlatformFile target_dir_handle = base::kInvalidPlatformFileValue;
  string16 origin_identifier;
  string16 database_name;
  if (vfs_file_name.empty()) {
    VfsBackend::OpenTempFileInDirectory(db_tracker_->DatabaseDirectory(),
                                        desired_flags, process_handle_,
                                        &target_handle, &target_dir_handle);
  } else if (DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                            &database_name, NULL) &&
             !db_tracker_->IsDatabaseScheduledForDeletion(origin_identifier,
                                                          database_name)) {
      FilePath db_file =
          DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
      if (!db_file.empty()) {
        VfsBackend::OpenFile(db_file, desired_flags, process_handle_,
                             &target_handle, &target_dir_handle);
      }
  }

  ViewMsg_DatabaseOpenFileResponse_Params response_params;
  SetOpenFileResponseParams(&response_params, target_handle, target_dir_handle);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseOpenFileResponse(
                            message_id, response_params)));
}

void DatabaseDispatcherHost::OnDatabaseDeleteFile(const string16& vfs_file_name,
                                                  const bool& sync_dir,
                                                  int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseDeleteFile,
                        vfs_file_name,
                        sync_dir,
                        message_id,
                        kNumDeleteRetries));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
void DatabaseDispatcherHost::DatabaseDeleteFile(const string16& vfs_file_name,
                                                bool sync_dir,
                                                int32 message_id,
                                                int reschedule_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty()) {
    error_code = VfsBackend::DeleteFile(db_file, sync_dir);
    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      ChromeThread::PostDelayedTask(
          ChromeThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &DatabaseDispatcherHost::DatabaseDeleteFile,
                            vfs_file_name,
                            sync_dir,
                            message_id,
                            reschedule_count - 1),
          kDelayDeleteRetryMs);
      return;
    }
  }

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseDeleteFileResponse(
                            message_id, error_code)));
}

void DatabaseDispatcherHost::OnDatabaseGetFileAttributes(
    const string16& vfs_file_name,
    int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileAttributes,
                        vfs_file_name,
                        message_id));
}

// Scheduled by the IO thread on the file thread.
// Gets the attributes of the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileAttributes(
    const string16& vfs_file_name,
    int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int32 attributes = -1;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty())
    attributes = VfsBackend::GetFileAttributes(db_file);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseGetFileAttributesResponse(
                            message_id, attributes)));
}

void DatabaseDispatcherHost::OnDatabaseGetFileSize(
  const string16& vfs_file_name, int32 message_id) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileSize,
                        vfs_file_name,
                        message_id));
}

// Scheduled by the IO thread on the file thread.
// Gets the size of the given file, then schedules a task
// on the IO thread's message loop to send an IPC back to
// the corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileSize(const string16& vfs_file_name,
                                                 int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int64 size = 0;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty())
    size = VfsBackend::GetFileSize(db_file);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseGetFileSizeResponse(
                            message_id, size)));
}

void DatabaseDispatcherHost::OnDatabaseOpened(const string16& origin_identifier,
                                              const string16& database_name,
                                              const string16& description,
                                              int64 estimated_size) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseOpened,
                        origin_identifier,
                        database_name,
                        description,
                        estimated_size));
}

void DatabaseDispatcherHost::DatabaseOpened(const string16& origin_identifier,
                                            const string16& database_name,
                                            const string16& description,
                                            int64 estimated_size) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  int64 database_size = 0;
  int64 space_available = 0;
  database_connections_.AddConnection(origin_identifier, database_name);
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size, &space_available);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseUpdateSize(
                            origin_identifier, database_name,
                            database_size, space_available)));
}

void DatabaseDispatcherHost::OnDatabaseModified(
    const string16& origin_identifier,
    const string16& database_name) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseModified,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::DatabaseModified(const string16& origin_identifier,
                                              const string16& database_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    ReceivedBadMessage(ViewHostMsg_DatabaseModified::ID);
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void DatabaseDispatcherHost::OnDatabaseClosed(const string16& origin_identifier,
                                              const string16& database_name) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseClosed,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::DatabaseClosed(const string16& origin_identifier,
                                            const string16& database_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    ReceivedBadMessage(ViewHostMsg_DatabaseClosed::ID);
    return;
  }

  db_tracker_->DatabaseClosed(origin_identifier, database_name);
  database_connections_.RemoveConnection(origin_identifier, database_name);
}

void DatabaseDispatcherHost::OnDatabaseSizeChanged(
    const string16& origin_identifier,
    const string16& database_name,
    int64 database_size,
    int64 space_available) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (database_connections_.IsOriginUsed(origin_identifier)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &DatabaseDispatcherHost::SendMessage,
                          new ViewMsg_DatabaseUpdateSize(
                              origin_identifier, database_name,
                              database_size, space_available)));
  }
}

void DatabaseDispatcherHost::OnDatabaseScheduledForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::SendMessage,
                        new ViewMsg_DatabaseCloseImmediately(
                            origin_identifier, database_name)));
}

void DatabaseDispatcherHost::OnDatabaseOpenFileAllowed(
    const string16& vfs_file_name, int desired_flags, int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (shutdown_)
    return;

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseOpenFile,
                        vfs_file_name,
                        desired_flags,
                        message_id));
}

void DatabaseDispatcherHost::OnDatabaseOpenFileBlocked(int32 message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (shutdown_)
    return;

  // This will result in failed transactions NOT a failed window.openDatabase
  // call.
  ViewMsg_DatabaseOpenFileResponse_Params response_params;
  SetOpenFileResponseParams(&response_params,
                            base::kInvalidPlatformFileValue,
                            base::kInvalidPlatformFileValue);
  SendMessage(new ViewMsg_DatabaseOpenFileResponse(message_id,
                                                   response_params));
}
