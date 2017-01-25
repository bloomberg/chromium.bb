// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blacklist_client.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// static
void PermissionBlacklistClient::CheckSafeBrowsingBlacklist(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
    content::PermissionType permission_type,
    const GURL& request_origin,
    content::WebContents* web_contents,
    int timeout,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  new PermissionBlacklistClient(db_manager, permission_type, request_origin,
                                web_contents, timeout, callback);
}

PermissionBlacklistClient::PermissionBlacklistClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
    content::PermissionType permission_type,
    const GURL& request_origin,
    content::WebContents* web_contents,
    int timeout,
    base::Callback<void(bool)> callback)
    : content::WebContentsObserver(web_contents),
      db_manager_(db_manager),
      permission_type_(permission_type),
      callback_(callback),
      timeout_(timeout),
      is_active_(true) {
  // Balanced by a call to Release() in OnCheckApiBlacklistUrlResult().
  AddRef();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&PermissionBlacklistClient::StartCheck, this, request_origin));
}

PermissionBlacklistClient::~PermissionBlacklistClient() {}

void PermissionBlacklistClient::StartCheck(const GURL& request_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Start the timer to interrupt into the client callback method with an
  // empty response if Safe Browsing times out.
  safe_browsing::ThreatMetadata empty_metadata;
  timer_ = base::MakeUnique<base::OneShotTimer>();
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_),
      base::Bind(&PermissionBlacklistClient::OnCheckApiBlacklistUrlResult, this,
                 request_origin, empty_metadata));
  db_manager_->CheckApiBlacklistUrl(request_origin, this);
}

void PermissionBlacklistClient::OnCheckApiBlacklistUrlResult(
    const GURL& url,
    const safe_browsing::ThreatMetadata& metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (timer_->IsRunning())
    timer_->Stop();
  else
    db_manager_->CancelApiCheck(this);
  timer_.reset(nullptr);

  bool permission_blocked =
      metadata.api_permissions.find(
          PermissionUtil::ConvertPermissionTypeToSafeBrowsingName(
              permission_type_)) != metadata.api_permissions.end();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&PermissionBlacklistClient::EvaluateBlacklistResultOnUiThread,
                 this, permission_blocked));
}

void PermissionBlacklistClient::EvaluateBlacklistResultOnUiThread(
    bool permission_blocked) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_active_)
    callback_.Run(permission_blocked);
  Release();
}

void PermissionBlacklistClient::WebContentsDestroyed() {
  is_active_ = false;
  Observe(nullptr);
}
