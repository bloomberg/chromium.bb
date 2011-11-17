// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#pragma once

#include "base/hash_tables.h"
#include "base/string16.h"
#include "content/browser/browser_message_filter.h"
#include "webkit/database/database_connections.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_types.h"

class DatabaseMessageFilter
    : public BrowserMessageFilter,
      public webkit_database::DatabaseTracker::Observer {
 public:
  explicit DatabaseMessageFilter(webkit_database::DatabaseTracker* db_tracker);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

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

  // Quota message handler (io thread)
  void OnDatabaseGetSpaceAvailable(const string16& origin_identifier,
                                   IPC::Message* reply_msg);
  void OnDatabaseGetUsageAndQuota(IPC::Message* reply_msg,
                                  quota::QuotaStatusCode status,
                                  int64 usage,
                                  int64 quota);

  // Database tracker message handlers (file thread)
  void OnDatabaseOpened(const string16& origin_identifier,
                        const string16& database_name,
                        const string16& description,
                        int64 estimated_size);
  void OnDatabaseModified(const string16& origin_identifier,
                          const string16& database_name);
  void OnDatabaseClosed(const string16& origin_identifier,
                        const string16& database_name);

  // DatabaseTracker::Observer callbacks (file thread)
  virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                     const string16& database_name,
                                     int64 database_size) OVERRIDE;
  virtual void OnDatabaseScheduledForDeletion(
      const string16& origin_identifier,
      const string16& database_name) OVERRIDE;

  void DatabaseDeleteFile(const string16& vfs_file_name,
                          bool sync_dir,
                          IPC::Message* reply_msg,
                          int reschedule_count);

  // The database tracker for the current browser context.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  // True if and only if this instance was added as an observer
  // to DatabaseTracker.
  bool observer_added_;

  // Keeps track of all DB connections opened by this renderer
  webkit_database::DatabaseConnections database_connections_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
