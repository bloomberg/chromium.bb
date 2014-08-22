// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_MESSAGE_FILTER_H_
#define CONTENT_SHELL_BROWSER_SHELL_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class QuotaManager;
}

namespace storage {
class DatabaseTracker;
}

namespace content {

class ShellMessageFilter : public BrowserMessageFilter {
 public:
  ShellMessageFilter(int render_process_id,
                     storage::DatabaseTracker* database_tracker,
                     storage::QuotaManager* quota_manager,
                     net::URLRequestContextGetter* request_context_getter);

 private:
  virtual ~ShellMessageFilter();

  // BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnReadFileToString(const base::FilePath& local_file,
                          std::string* contents);
  void OnRegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& absolute_filenames,
      std::string* filesystem_id);
  void OnClearAllDatabases();
  void OnSetDatabaseQuota(int quota);
  void OnCheckWebNotificationPermission(const GURL& origin, int* result);
  void OnGrantWebNotificationPermission(const GURL& origin,
                                        bool permission_granted);
  void OnClearWebNotificationPermissions();
  void OnAcceptAllCookies(bool accept);
  void OnDeleteAllCookies();

  int render_process_id_;

  storage::DatabaseTracker* database_tracker_;
  storage::QuotaManager* quota_manager_;
  net::URLRequestContextGetter* request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ShellMessageFilter);
};

}  // namespace content

#endif // CONTENT_SHELL_BROWSER_SHELL_MESSAGE_FILTER_H_
