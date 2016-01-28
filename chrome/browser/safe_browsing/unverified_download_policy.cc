// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_policy.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/unverified_download_field_trial.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {
using content::BrowserThread;

// Record the uma_file_type to UMA as an enum, and the URL's eTLD+1 to Rappor.
void RecordPolicyMetricOnUIThread(const std::string& metric_name,
                                  int uma_file_type,
                                  const GURL& requestor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_SPARSE_SLOWLY(metric_name, uma_file_type);

  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  if (rappor_service && requestor.is_valid()) {
    rappor_service->RecordSample(
        metric_name, rappor::SAFEBROWSING_RAPPOR_TYPE,
        rappor::GetDomainAndRegistrySampleFromGURL(requestor));
  }
}

void RecordPolicyMetric(const std::string& metric_name,
                        int uma_file_type,
                        const GURL& requestor) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RecordPolicyMetricOnUIThread, metric_name,
                                     uma_file_type, requestor));
}

void RespondWithPolicy(
    const base::FilePath& file,
    const UnverifiedDownloadCheckCompletionCallback& callback,
    const GURL& requestor,
    UnverifiedDownloadPolicy policy) {
  int uma_file_type =
      download_protection_util::GetSBClientDownloadExtensionValueForUMA(file);
  if (policy == UnverifiedDownloadPolicy::ALLOWED) {
    RecordPolicyMetric("SafeBrowsing.UnverifiedDownloads.Allowed",
                       uma_file_type, requestor);
  } else {
    RecordPolicyMetric("SafeBrowsing.UnverifiedDownloads.Blocked",
                       uma_file_type, requestor);
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, policy));
}

void CheckFieldTrialOnAnyThread(
    const base::FilePath& file,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    const GURL& requestor,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  if (!IsUnverifiedDownloadAllowedByFieldTrial(file)) {
    RespondWithPolicy(file, callback, requestor,
                      UnverifiedDownloadPolicy::DISALLOWED);
    return;
  }

  for (const auto& extension : alternate_extensions) {
    base::FilePath alternate_filename = file.AddExtension(extension);
    if (!IsUnverifiedDownloadAllowedByFieldTrial(alternate_filename)) {
      RespondWithPolicy(alternate_filename, callback, requestor,
                        UnverifiedDownloadPolicy::DISALLOWED);
      return;
    }
  }

  RespondWithPolicy(file, callback, requestor,
                    UnverifiedDownloadPolicy::ALLOWED);
}

void CheckWhitelistOnIOThread(
    scoped_refptr<SafeBrowsingService> service,
    const GURL& requestor,
    const base::FilePath& file,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int uma_file_type =
      download_protection_util::GetSBClientDownloadExtensionValueForUMA(file);

  if (!service || !service->enabled()) {
    // If the SafeBrowsing service was disabled, don't try to check against the
    // field trial list. Instead allow the download. We are assuming that if the
    // SafeBrowsing service was disabled for this user, then we shouldn't
    // interefere with unverified downloads.
    RecordPolicyMetric(
        "SafeBrowsing.UnverifiedDownloads.AllowedDueToDisabledService",
        uma_file_type, requestor);
    RespondWithPolicy(file, callback, requestor,
                      UnverifiedDownloadPolicy::ALLOWED);
    return;
  }

  if (service->database_manager() &&
      service->database_manager()->MatchDownloadWhitelistUrl(requestor)) {
    RecordPolicyMetric("SafeBrowsing.UnverifiedDownloads.AllowedByWhitelist",
                       uma_file_type, requestor);
    RespondWithPolicy(file, callback, requestor,
                      UnverifiedDownloadPolicy::ALLOWED);
    return;
  }

  CheckFieldTrialOnAnyThread(file, alternate_extensions, requestor, callback);
}

}  // namespace

void CheckUnverifiedDownloadPolicy(
    const GURL& requestor,
    const base::FilePath& file,
    const std::vector<base::FilePath::StringType>& alternate_extensions,
    const UnverifiedDownloadCheckCompletionCallback& callback) {
  // Record a count of alternate extensions. The count is capped so that it
  // won't be unbounded. In practice, it's expected that numbers above 3 are
  // rare.
  int alternate_extension_count =
      std::min<int>(alternate_extensions.size(), 16);
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "SafeBrowsing.UnverifiedDownloads.AlternateExtensionCount",
      alternate_extension_count);
  if (requestor.is_valid()) {
    scoped_refptr<SafeBrowsingService> service =
        g_browser_process->safe_browsing_service();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CheckWhitelistOnIOThread, service, requestor, file,
                   alternate_extensions, callback));
    return;
  }

  CheckFieldTrialOnAnyThread(file, alternate_extensions, GURL(), callback);
}

}  // namespace safe_browsing
