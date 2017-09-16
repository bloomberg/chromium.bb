// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/strings/string16.h"
#include "content/common/web_database.mojom.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/common/database/database_connections.h"
#include "storage/common/quota/quota_types.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class DatabaseMessageFilter : public BrowserMessageFilter,
                              public storage::DatabaseTracker::Observer {
 public:
  explicit DatabaseMessageFilter(int process_id,
                                 storage::DatabaseTracker* db_tracker);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  storage::DatabaseTracker* database_tracker() const {
    return db_tracker_.get();
  }

 private:
  ~DatabaseMessageFilter() override;

  class PromptDelegate;

  void AddObserver();
  void RemoveObserver();

  // VFS message handlers (tracker sequence)

  // Database tracker message handlers (tracker sequence)
  void OnDatabaseOpened(const url::Origin& origin,
                        const base::string16& database_name,
                        const base::string16& description,
                        int64_t estimated_size);
  void OnDatabaseModified(const url::Origin& origin,
                          const base::string16& database_name);
  void OnDatabaseClosed(const url::Origin& origin,
                        const base::string16& database_name);
  void OnHandleSqliteError(const url::Origin& origin,
                           const base::string16& database_name,
                           int error);

  // DatabaseTracker::Observer callbacks (tracker sequence)
  void OnDatabaseSizeChanged(const std::string& origin_identifier,
                             const base::string16& database_name,
                             int64_t database_size) override;
  void OnDatabaseScheduledForDeletion(
      const std::string& origin_identifier,
      const base::string16& database_name) override;

  void DatabaseDeleteFile(const base::string16& vfs_file_name,
                          bool sync_dir,
                          IPC::Message* reply_msg,
                          int reschedule_count);

  // Callback used to close the connection to the WebDatabase mojo interface on
  // the db_tracker task runner.
  void CloseOnDatabaseTrackerTask();

  // Helper function to get the mojo interface for the WebDatabase on the
  // render process. Creates the WebDatabase connection if it does not already
  // exist.
  content::mojom::WebDatabase& GetWebDatabase();

  // Our render process host ID, used to bind to the correct render process.
  const int process_id_;

  // The database tracker for the current browser context.
  scoped_refptr<storage::DatabaseTracker> db_tracker_;

  // True if and only if this instance was added as an observer
  // to DatabaseTracker.
  bool observer_added_;

  // Keeps track of all DB connections opened by this renderer
  storage::DatabaseConnections database_connections_;

  // Interface to the render process WebDatabase.
  content::mojom::WebDatabasePtr database_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
