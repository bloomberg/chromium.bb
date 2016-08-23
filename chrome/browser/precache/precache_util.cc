// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/precache/precache_util.h"

#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_use_measurement/content/data_use_measurement.h"
#include "components/precache/content/precache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class HttpResponseInfo;
}

namespace {

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
      url, referrer, latency, fetch_time, info, size, is_user_traffic);
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
      base::Bind(
          &UpdatePrecacheMetricsAndStateOnUIThread, request->url(),
          GURL(request->referrer()), latency, base::Time::Now(),
          request->response_info(), received_content_length,
          data_use_measurement::DataUseMeasurement::IsUserInitiatedRequest(
              *request),
          profile_id));
}

}  // namespace precache
