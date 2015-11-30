// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace google_captcha_observer {

namespace {

const char kGoogleCaptchaEvents[] = "PageLoad.Clients.GoogleCaptcha.Events";

enum GoogleCaptchaEvent {
  // A Google CAPTCHA page was shown to the user.
  GOOGLE_CAPTCHA_SHOWN,
  // A Google CAPTCHA page was solved by the user.
  GOOGLE_CAPTCHA_SOLVED,
  // Add new values before this final count.
  GOOGLE_CAPTCHA_EVENT_BOUNDARY,
};

void RecordGoogleCaptchaEvent(GoogleCaptchaEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      kGoogleCaptchaEvents, event,
      GOOGLE_CAPTCHA_EVENT_BOUNDARY);
}

}  // namespace

bool IsGoogleCaptcha(const GURL& url) {
  return (base::StartsWith(url.host(), "ipv4.google.",
                           base::CompareCase::SENSITIVE)
          || base::StartsWith(url.host(), "ipv6.google.",
                              base::CompareCase::SENSITIVE))
      && base::StartsWith(url.path(), "/sorry", base::CompareCase::SENSITIVE);
}

GoogleCaptchaObserver::GoogleCaptchaObserver(
    page_load_metrics::PageLoadMetricsObservable* metrics)
    : saw_solution_(false), metrics_(metrics) {}

void GoogleCaptchaObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (IsGoogleCaptcha(navigation_handle->GetURL()))
    RecordGoogleCaptchaEvent(GOOGLE_CAPTCHA_SHOWN);
  if (saw_solution_) {
    RecordGoogleCaptchaEvent(GOOGLE_CAPTCHA_SOLVED);
    saw_solution_ = false;
  }
}

void GoogleCaptchaObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  if (IsGoogleCaptcha(navigation_handle->GetReferrer().url))
    saw_solution_ = true;
}

void GoogleCaptchaObserver::OnPageLoadMetricsGoingAway() {
  metrics_->RemoveObserver(this);
  delete this;
}

}  // namespace google_captcha_observer
