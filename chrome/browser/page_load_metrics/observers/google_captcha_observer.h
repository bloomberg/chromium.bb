// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GOOGLE_CAPTCHA_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GOOGLE_CAPTCHA_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace google_captcha_observer {

// Returns true if the given URL matches a Google CAPTCHA page.
bool IsGoogleCaptcha(const GURL& url);

class GoogleCaptchaObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit GoogleCaptchaObserver(
      page_load_metrics::PageLoadMetricsObservable* metrics);

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnRedirect(content::NavigationHandle* navigation_handle) override;
  void OnPageLoadMetricsGoingAway() override;

 private:
  bool saw_solution_;
  page_load_metrics::PageLoadMetricsObservable* const metrics_;
  DISALLOW_COPY_AND_ASSIGN(GoogleCaptchaObserver);

};

}  // namespace google_captcha_observer

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GOOGLE_CAPTCHA_OBSERVER_H_
