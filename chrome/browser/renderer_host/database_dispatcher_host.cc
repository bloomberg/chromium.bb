// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_dispatcher_host.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "third_party/sqlite/sqlite3.h"
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
    IPC::Message::Sender* sender,
    HostContentSettingsMap *host_content_settings_map)
    : db_tracker_(db_tracker),
      message_sender_(sender),
      process_handle_(0),
      observer_added_(false),
      shutdown_(false),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(db_tracker_);
  DCHECK(message_sender_);
}

void DatabaseDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
}

void DatabaseDispatcherHost::Shutdown() {
  shutdown_ = true;
  message_sender_ = NULL;
  if (observer_added_) {
    observer_added_ = false;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::RemoveObserver));
  }
}

void DatabaseDispatcherHost::AddObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  db_tracker_->AddObserver(this);
}

void DatabaseDispatcherHost::RemoveObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DatabaseOpenFile,
                                    OnDatabaseOpenFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DatabaseDeleteFile,
                                    OnDatabaseDeleteFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DatabaseGetFileAttributes,
                                    OnDatabaseGetFileAttributes)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DatabaseGetFileSize,
                                    OnDatabaseGetFileSize)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseOpened, OnDatabaseOpened)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseModified, OnDatabaseModified)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DatabaseClosed, OnDatabaseClosed)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DatabaseDispatcherHost::ReceivedBadMessage(uint32 msg_type) {
  BrowserRenderProcessHost::BadMessageTerminateProcess(
      msg_type, process_handle_);
}

void DatabaseDispatcherHost::Send(IPC::Message* message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            NewRunnableMethod(this,
                              &DatabaseDispatcherHost::Send,
                              message)))
      delete message;
    return;
  }

  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}

DatabaseDispatcherHost::~DatabaseDispatcherHost() {}

void DatabaseDispatcherHost::OnDatabaseOpenFile(const string16& vfs_file_name,
                                                int desired_flags,
                                                IPC::Message* reply_msg) {
  if (!observer_added_) {
    observer_added_ = true;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &DatabaseDispatcherHost::AddObserver));
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseOpenFile,
                        vfs_file_name,
                        desired_flags,
                        reply_msg));
}

// Scheduled by the IO thread on the file thread.
// Opens the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the file handle.
void DatabaseDispatcherHost::DatabaseOpenFile(const string16& vfs_file_name,
                                              int desired_flags,
                                              IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  base::PlatformFile target_handle = base::kInvalidPlatformFileValue;
  string16 origin_identifier;
  string16 database_name;

  // When in incognito mode, we want to make sure that all DB files are
  // removed when the incognito profile goes away, so we add the
  // SQLITE_OPEN_DELETEONCLOSE flag when opening all files, and keep
  // open handles to them in the database tracker to make sure they're
  // around for as long as needed.
  if (vfs_file_name.empty()) {
    VfsBackend::OpenTempFileInDirectory(db_tracker_->DatabaseDirectory(),
                                        desired_flags, &file_handle);
  } else if (DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                            &database_name, NULL) &&
             !db_tracker_->IsDatabaseScheduledForDeletion(origin_identifier,
                                                          database_name)) {
      FilePath db_file =
          DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
      if (!db_file.empty()) {
        if (db_tracker_->IsIncognitoProfile()) {
          db_tracker_->GetIncognitoFileHandle(vfs_file_name, &file_handle);
          if (file_handle == base::kInvalidPlatformFileValue) {
            VfsBackend::OpenFile(db_file,
                                 desired_flags | SQLITE_OPEN_DELETEONCLOSE,
                                 &file_handle);
            if (VfsBackend::FileTypeIsMainDB(desired_flags) ||
                VfsBackend::FileTypeIsJournal(desired_flags))
              db_tracker_->SaveIncognitoFileHandle(vfs_file_name, file_handle);
          }
        } else {
          VfsBackend::OpenFile(db_file, desired_flags, &file_handle);
        }
      }
  }

  // Then we duplicate the file handle to make it useable in the renderer
  // process. The original handle is closed, unless we saved it in the
  // database tracker.
  bool auto_close = !db_tracker_->HasSavedIncognitoFileHandle(vfs_file_name);
  VfsBackend::GetFileHandleForProcess(process_handle_, file_handle,
                                      &target_handle, auto_close);

  ViewHostMsg_DatabaseOpenFile::WriteReplyParams(
      reply_msg,
#if defined(OS_WIN)
      target_handle
#elif defined(OS_POSIX)
      base::FileDescriptor(target_handle, auto_close)
#endif
      );
  Send(reply_msg);
}

void DatabaseDispatcherHost::OnDatabaseDeleteFile(const string16& vfs_file_name,
                                                  const bool& sync_dir,
                                                  IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseDeleteFile,
                        vfs_file_name,
                        sync_dir,
                        reply_msg,
                        kNumDeleteRetries));
}

// Scheduled by the IO thread on the file thread.
// Deletes the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process with the error code.
void DatabaseDispatcherHost::DatabaseDeleteFile(const string16& vfs_file_name,
                                                bool sync_dir,
                                                IPC::Message* reply_msg,
                                                int reschedule_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty()) {
    // In order to delete a journal file in incognito mode, we only need to
    // close the open handle to it that's stored in the database tracker.
    if (db_tracker_->IsIncognitoProfile()) {
      if (db_tracker_->CloseIncognitoFileHandle(vfs_file_name))
        error_code = SQLITE_OK;
    } else {
      error_code = VfsBackend::DeleteFile(db_file, sync_dir);
    }

    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      BrowserThread::PostDelayedTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &DatabaseDispatcherHost::DatabaseDeleteFile,
                            vfs_file_name,
                            sync_dir,
                            reply_msg,
                            reschedule_count - 1),
          kDelayDeleteRetryMs);
      return;
    }
  }

  ViewHostMsg_DatabaseDeleteFile::WriteReplyParams(reply_msg, error_code);
  Send(reply_msg);
}

void DatabaseDispatcherHost::OnDatabaseGetFileAttributes(
    const string16& vfs_file_name,
    IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileAttributes,
                        vfs_file_name,
                        reply_msg));
}

// Scheduled by the IO thread on the file thread.
// Gets the attributes of the given database file, then schedules
// a task on the IO thread's message loop to send an IPC back to
// corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileAttributes(
    const string16& vfs_file_name,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int32 attributes = -1;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty())
    attributes = VfsBackend::GetFileAttributes(db_file);

  ViewHostMsg_DatabaseGetFileAttributes::WriteReplyParams(
      reply_msg, attributes);
  Send(reply_msg);
}

void DatabaseDispatcherHost::OnDatabaseGetFileSize(
  const string16& vfs_file_name, IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseGetFileSize,
                        vfs_file_name,
                        reply_msg));
}

// Scheduled by the IO thread on the file thread.
// Gets the size of the given file, then schedules a task
// on the IO thread's message loop to send an IPC back to
// the corresponding renderer process.
void DatabaseDispatcherHost::DatabaseGetFileSize(const string16& vfs_file_name,
                                                 IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int64 size = 0;
  FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);
  if (!db_file.empty())
    size = VfsBackend::GetFileSize(db_file);

  ViewHostMsg_DatabaseGetFileSize::WriteReplyParams(reply_msg, size);
  Send(reply_msg);
}

void DatabaseDispatcherHost::OnDatabaseOpened(const string16& origin_identifier,
                                              const string16& database_name,
                                              const string16& description,
                                              int64 estimated_size) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int64 database_size = 0;
  int64 space_available = 0;
  database_connections_.AddConnection(origin_identifier, database_name);
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size, &space_available);
  Send(new ViewMsg_DatabaseUpdateSize(origin_identifier, database_name,
                                      database_size, space_available));
}

void DatabaseDispatcherHost::OnDatabaseModified(
    const string16& origin_identifier,
    const string16& database_name) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseModified,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::DatabaseModified(const string16& origin_identifier,
                                              const string16& database_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!database_connections_.IsDatabaseOpened(
          origin_identifier, database_name)) {
    ReceivedBadMessage(ViewHostMsg_DatabaseModified::ID);
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void DatabaseDispatcherHost::OnDatabaseClosed(const string16& origin_identifier,
                                              const string16& database_name) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &DatabaseDispatcherHost::DatabaseClosed,
                        origin_identifier,
                        database_name));
}

void DatabaseDispatcherHost::OnAllowDatabase(const std::string& origin_url,
                                             const string16& name,
                                             const string16& display_name,
                                             unsigned long estimated_size,
                                             IPC::Message* reply_msg) {
  GURL url = GURL(origin_url);
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  AllowDatabaseResponse(reply_msg, content_setting);
}

void DatabaseDispatcherHost::AllowDatabaseResponse(
    IPC::Message* reply_msg, ContentSetting content_setting) {
  DCHECK((content_setting == CONTENT_SETTING_ALLOW) ||
         (content_setting == CONTENT_SETTING_BLOCK) ||
         (content_setting == CONTENT_SETTING_SESSION_ONLY));
  ViewHostMsg_AllowDatabase::WriteReplyParams(
      reply_msg, content_setting != CONTENT_SETTING_BLOCK);
  Send(reply_msg);
}

void DatabaseDispatcherHost::DatabaseClosed(const string16& origin_identifier,
                                            const string16& database_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (database_connections_.IsOriginUsed(origin_identifier)) {
    Send(new ViewMsg_DatabaseUpdateSize(origin_identifier, database_name,
                                        database_size, space_available));
  }
}

void DatabaseDispatcherHost::OnDatabaseScheduledForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Send(new ViewMsg_DatabaseCloseImmediately(origin_identifier, database_name));
}
