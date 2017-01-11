// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/permissions/permission_util.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

namespace base {
class OneShotTimer;
}

// The client used when checking whether a permission has been blacklisted by
// Safe Browsing. The check is done asynchronously as no state can be stored in
// PermissionContextBase (since additional permission requests may be made).
// This class must be created and destroyed on the UI thread.
class PermissionBlacklistClient
    : public safe_browsing::SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<PermissionBlacklistClient>,
      public content::WebContentsObserver {
 public:
  static void CheckSafeBrowsingBlacklist(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      content::PermissionType permission_type,
      const GURL& request_origin,
      content::WebContents* web_contents,
      int timeout,
      base::Callback<void(bool)> callback);

 private:
  friend class base::RefCountedThreadSafe<PermissionBlacklistClient>;

  PermissionBlacklistClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      content::PermissionType permission_type,
      const GURL& request_origin,
      content::WebContents* web_contents,
      int timeout,
      base::Callback<void(bool)> callback);

  ~PermissionBlacklistClient() override;

  void StartCheck(const GURL& request_origin);

  // SafeBrowsingDatabaseManager::Client implementation.
  void OnCheckApiBlacklistUrlResult(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata) override;

  void EvaluateBlacklistResultOnUiThread(bool permission_blocked);

  // WebContentsObserver implementation. Sets a flag so that when the database
  // manager returns with a result, it won't attempt to run the callback with a
  // deleted WebContents.
  void WebContentsDestroyed() override;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager_;
  content::PermissionType permission_type_;

  // PermissionContextBase callback to run on the UI thread.
  base::Callback<void(bool)> callback_;

  // Timer to abort the Safe Browsing check if it takes too long. Created and
  // used on the IO Thread.
  std::unique_ptr<base::OneShotTimer> timer_;
  int timeout_;

  // True if |callback_| should be invoked, if web_contents() is destroyed, this
  // is set to false.
  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBlacklistClient);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_
