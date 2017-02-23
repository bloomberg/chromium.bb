// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

namespace base {
class OneShotTimer;
class ElapsedTimer;
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
  // |callback| will not be called if |web_contents| is destroyed. Thus if the
  // callback is run, the profile associated with |web_contents| is guaranteed
  // to be alive.
  static void CheckSafeBrowsingBlacklist(
      content::WebContents* web_contents,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      const GURL& request_origin,
      ContentSettingsType content_settings_type,
      int timeout,
      base::Callback<void(bool)> callback);

 private:
  friend class base::RefCountedThreadSafe<PermissionBlacklistClient>;

  PermissionBlacklistClient(
      content::WebContents* web_contents,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      const GURL& request_origin,
      ContentSettingsType content_settings_type,
      int timeout,
      base::Callback<void(bool)> callback);

  ~PermissionBlacklistClient() override;

  void StartCheck(const GURL& request_origin);

  // SafeBrowsingDatabaseManager::Client implementation.
  void OnCheckApiBlacklistUrlResult(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata) override;

  void EvaluateBlacklistResultOnUiThread(bool response);

  // WebContentsObserver implementation. Sets a flag so that when the database
  // manager returns with a result, it won't attempt to run the callback with a
  // deleted WebContents.
  void WebContentsDestroyed() override;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager_;
  ContentSettingsType content_settings_type_;

  // PermissionContextBase callback to run on the UI thread.
  base::Callback<void(bool)> callback_;

  // Timer to abort the Safe Browsing check if it takes too long. Created and
  // used on the IO Thread.
  std::unique_ptr<base::OneShotTimer> timer_;
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;
  int timeout_;

  // True if |callback_| should be invoked, if web_contents() is destroyed, this
  // is set to false.
  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBlacklistClient);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_BLACKLIST_CLIENT_H_
