// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blacklist_client.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// static
void PermissionBlacklistClient::CheckSafeBrowsingBlacklist(
    content::WebContents* web_contents,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    int timeout,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  new PermissionBlacklistClient(web_contents, db_manager, request_origin,
                                content_settings_type, timeout, callback);
}

PermissionBlacklistClient::PermissionBlacklistClient(
    content::WebContents* web_contents,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    int timeout,
    base::Callback<void(bool)> callback)
    : content::WebContentsObserver(web_contents),
      db_manager_(db_manager),
      content_settings_type_(content_settings_type),
      callback_(callback),
      timeout_(timeout),
      is_active_(true) {
  // Balanced by a call to Release() in EvaluateBlacklistResultOnUiThread().
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
  elapsed_timer_.reset(new base::ElapsedTimer());
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_),
      base::Bind(&PermissionBlacklistClient::OnCheckApiBlacklistUrlResult, this,
                 request_origin, empty_metadata));
  // If CheckApiBlacklistUrl returns true, no asynchronous call to |this| will
  // be made, so just directly call through to OnCheckApiBlacklistUrlResult.
  if (db_manager_->CheckApiBlacklistUrl(request_origin, this))
    OnCheckApiBlacklistUrlResult(request_origin, empty_metadata);
}

void PermissionBlacklistClient::OnCheckApiBlacklistUrlResult(
    const GURL& url,
    const safe_browsing::ThreatMetadata& metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::TimeDelta response_time = elapsed_timer_->Elapsed();
  SafeBrowsingResponse response = SafeBrowsingResponse::NOT_BLACKLISTED;

  if (timer_->IsRunning()) {
    timer_->Stop();
  } else {
    db_manager_->CancelApiCheck(this);
    response = SafeBrowsingResponse::TIMEOUT;
  }

  timer_.reset(nullptr);
  bool permission_blocked =
      metadata.api_permissions.find(
          PermissionUtil::ConvertContentSettingsTypeToSafeBrowsingName(
              content_settings_type_)) != metadata.api_permissions.end();
  if (permission_blocked)
    response = SafeBrowsingResponse::BLACKLISTED;

  PermissionUmaUtil::RecordSafeBrowsingResponse(response_time, response);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&PermissionBlacklistClient::EvaluateBlacklistResultOnUiThread,
                 this, permission_blocked));
}

void PermissionBlacklistClient::EvaluateBlacklistResultOnUiThread(
    bool response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_active_)
    callback_.Run(response);
  Release();
}

void PermissionBlacklistClient::WebContentsDestroyed() {
  is_active_ = false;
  Observe(nullptr);
}
