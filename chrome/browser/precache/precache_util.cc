// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/precache/precache_util.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_use_measurement/content/content_url_request_classifier.h"
#include "components/precache/content/precache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class HttpResponseInfo;
}

namespace {

const char kPrecacheSynthetic15D[] = "PrecacheSynthetic15D";
const char kPrecacheSynthetic1D[] = "PrecacheSynthetic1D";

void UpdatePrecacheMetricsAndStateOnUIThread(const GURL& url,
                                             const GURL& referrer,
                                             base::TimeDelta latency,
                                             const base::Time& fetch_time,
                                             const net::HttpResponseInfo& info,
                                             int64_t size,
                                             bool is_user_traffic,
                                             void* profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  precache::PrecacheManager* precache_manager =
      precache::PrecacheManagerFactory::GetForBrowserContext(profile);
  // |precache_manager| could be NULL if the profile is off the record.
  if (!precache_manager || !precache_manager->IsPrecachingAllowed())
    return;

  precache_manager->UpdatePrecacheMetricsAndState(
      url, referrer, latency, fetch_time, info, size, is_user_traffic,
      base::Bind(&precache::RegisterPrecacheSyntheticFieldTrial));
}

}  // namespace

namespace precache {

// TODO(rajendrant): Add unittests for this function.
void UpdatePrecacheMetricsAndState(const net::URLRequest* request,
                                   void* profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // For better accuracy, we use the actual bytes read instead of the length
  // specified with the Content-Length header, which may be inaccurate,
  // or missing, as is the case with chunked encoding.
  int64_t received_content_length = request->received_response_content_length();
  base::TimeDelta latency = base::TimeTicks::Now() - request->creation_time();

  // Record precache metrics when a fetch is completed successfully, if
  // precaching is allowed.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdatePrecacheMetricsAndStateOnUIThread, request->url(),
                 GURL(request->referrer()), latency, base::Time::Now(),
                 request->response_info(), received_content_length,
                 data_use_measurement::IsUserRequest(*request), profile_id));
}

// |last_precache_time| is the last time precache task was run.
void RegisterPrecacheSyntheticFieldTrial(base::Time last_precache_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<uint32_t> groups;
  base::TimeDelta time_ago = base::Time::Now() - last_precache_time;
  // Look up the current group name (e.g. Control or Enabled).
  std::string group_name =
      base::FieldTrialList::FindFullName(kPrecacheFieldTrialName);
  // group_name should only be empty if the Precache trial does not exist.
  if (!group_name.empty()) {
    // Register matching synthetic trials for 15-day and 1-day candidates.
    if (time_ago <= base::TimeDelta::FromDays(15))
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kPrecacheSynthetic15D, group_name);
    if (time_ago <= base::TimeDelta::FromDays(1))
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kPrecacheSynthetic1D, group_name);
  }
}

}  // namespace precache
