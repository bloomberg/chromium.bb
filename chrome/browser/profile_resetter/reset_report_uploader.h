// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace content {
class BrowserContext;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace reset_report {
class ChromeResetReport;
}

// Service whose job is up upload ChromeResetReports.
class ResetReportUploader : public KeyedService,
                            private net::URLFetcherDelegate {
 public:
  explicit ResetReportUploader(content::BrowserContext* context);
  ~ResetReportUploader() override;

  void DispatchReport(const reset_report::ChromeResetReport& report);

 private:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ResetReportUploader);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_RESET_REPORT_UPLOADER_H_
