// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_policy.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/unverified_download_field_trial.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

void RespondWithPolicy(
    const base::FilePath& file,
    const UnverifiedDownloadCheckCompletionCallback& callback,
    UnverifiedDownloadPolicy policy) {
  int uma_file_type =
      download_protection_util::GetSBClientDownloadExtensionValueForUMA(file);
  if (policy == UnverifiedDownloadPolicy::ALLOWED) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.UnverifiedDownloads.Allowed",
                                uma_file_type);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.UnverifiedDownloads.Blocked",
                                uma_file_type);
  }
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, policy));
}

void CheckFieldTrialOnAnyThread(
    const base::FilePath& file,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  bool is_allowed = IsUnverifiedDownloadAllowedByFieldTrial(file);
  RespondWithPolicy(file, callback, is_allowed
                                        ? UnverifiedDownloadPolicy::ALLOWED
                                        : UnverifiedDownloadPolicy::DISALLOWED);
}

void CheckWhitelistOnIOThread(
    scoped_refptr<SafeBrowsingService> service,
    const GURL& requestor,
    const base::FilePath& file,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  int uma_file_type =
      download_protection_util::GetSBClientDownloadExtensionValueForUMA(file);

  if (!service || !service->enabled()) {
    // If the SafeBrowsing service was disabled, don't try to check against the
    // field trial list. Instead allow the download. We are assuming that if the
    // SafeBrowsing service was disabled for this user, then we shouldn't
    // interefere with unverified downloads.
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SafeBrowsing.UnverifiedDownloads.AllowedDueToDisabledService",
        uma_file_type);
    RespondWithPolicy(file, callback, UnverifiedDownloadPolicy::ALLOWED);
    return;
  }

  if (service->database_manager() &&
      service->database_manager()->MatchDownloadWhitelistUrl(requestor)) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SafeBrowsing.UnverifiedDownloads.AllowedByWhitelist", uma_file_type);
    RespondWithPolicy(file, callback, UnverifiedDownloadPolicy::ALLOWED);
    return;
  }

  CheckFieldTrialOnAnyThread(file, callback);
}

}  // namespace

void CheckUnverifiedDownloadPolicy(
    const GURL& requestor,
    const base::FilePath& file,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  if (requestor.is_valid()) {
    scoped_refptr<SafeBrowsingService> service =
        g_browser_process->safe_browsing_service();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&CheckWhitelistOnIOThread, service, requestor, file,
                   callback));
    return;
  }

  CheckFieldTrialOnAnyThread(file, callback);
}

}  // namespace safe_browsing
