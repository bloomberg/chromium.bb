// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#pragma once

#include "base/hash_tables.h"
#include "base/string16.h"
#include "chrome/browser/browser_message_filter.h"
#include "chrome/common/content_settings.h"
#include "webkit/database/database_connections.h"
#include "webkit/database/database_tracker.h"

class HostContentSettingsMap;

class DatabaseMessageFilter
    : public BrowserMessageFilter,
      public webkit_database::DatabaseTracker::Observer {
 public:
  DatabaseMessageFilter(
      webkit_database::DatabaseTracker* db_tracker,
      HostContentSettingsMap *host_content_settings_map);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  webkit_database::DatabaseTracker* database_tracker() const {
    return db_tracker_.get();
  }

 private:
  virtual ~DatabaseMessageFilter();

  class PromptDelegate;

  void AddObserver();
  void RemoveObserver();

  // VFS message handlers (file thread)
  void OnDatabaseOpenFile(const string16& vfs_file_name,
                          int desired_flags,
                          IPC::Message* reply_msg);
  void OnDatabaseDeleteFile(const string16& vfs_file_name,
                            const bool& sync_dir,
                            IPC::Message* reply_msg);
  void OnDatabaseGetFileAttributes(const string16& vfs_file_name,
                                   IPC::Message* reply_msg);
  void OnDatabaseGetFileSize(const string16& vfs_file_name,
                             IPC::Message* reply_msg);

  // Database tracker message handlers (file thread)
  void OnDatabaseOpened(const string16& origin_identifier,
                        const string16& database_name,
                        const string16& description,
                        int64 estimated_size);
  void OnDatabaseModified(const string16& origin_identifier,
                          const string16& database_name);
  void OnDatabaseClosed(const string16& origin_identifier,
                        const string16& database_name);
  void OnAllowDatabase(const std::string& origin_url,
                       const string16& name,
                       const string16& display_name,
                       unsigned long estimated_size,
                       IPC::Message* reply_msg);

  // DatabaseTracker::Observer callbacks (file thread)
  virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                     const string16& database_name,
                                     int64 database_size,
                                     int64 space_available);
  virtual void OnDatabaseScheduledForDeletion(const string16& origin_identifier,
                                              const string16& database_name);

  void DatabaseDeleteFile(const string16& vfs_file_name,
                          bool sync_dir,
                          IPC::Message* reply_msg,
                          int reschedule_count);

  // CookiePromptModalDialog response handler (io thread)
  void AllowDatabaseResponse(IPC::Message* reply_msg,
                             ContentSetting content_setting);

  // The database tracker for the current profile.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  // True if and only if this instance was added as an observer
  // to DatabaseTracker.
  bool observer_added_;

  // Keeps track of all DB connections opened by this renderer
  webkit_database::DatabaseConnections database_connections_;

  // Used to look up permissions at database creation time.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
