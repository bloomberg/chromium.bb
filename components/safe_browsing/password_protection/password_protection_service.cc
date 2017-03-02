// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

PasswordProtectionService::PasswordProtectionService(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : database_manager_(database_manager), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PasswordProtectionService::~PasswordProtectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void PasswordProtectionService::RecordPasswordReuse(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(database_manager_);
  if (!url.is_valid())
    return;

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManager::MatchCsdWhitelistUrl,
                 database_manager_, url),
      base::Bind(&PasswordProtectionService::OnMatchCsdWhiteListResult,
                 GetWeakPtr()));
}

void PasswordProtectionService::OnMatchCsdWhiteListResult(
    bool match_whitelist) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist",
      match_whitelist);
}

}  // namespace safe_browsing
