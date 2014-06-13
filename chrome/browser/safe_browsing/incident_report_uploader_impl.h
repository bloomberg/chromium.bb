// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORT_UPLOADER_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORT_UPLOADER_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/incident_report_uploader.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

class ClientIncidentReport;

// An uploader of incident reports. A report is issued via the UploadReport
// method. The upload can be cancelled by deleting the returned instance. The
// instance is no longer usable after the delegate is notified, and may safely
// be destroyed by the delegate.
class IncidentReportUploaderImpl : public IncidentReportUploader,
                                   public net::URLFetcherDelegate {
 public:
  // The id associated with the URLFetcher, for use by tests.
  static const int kTestUrlFetcherId;

  virtual ~IncidentReportUploaderImpl();

  // Uploads a report with a caller-provided URL context. |callback| will be run
  // when the upload is complete. Returns NULL if |report| cannot be serialized
  // for transmission, in which case the delegate is not notified.
  static scoped_ptr<IncidentReportUploader> UploadReport(
      const OnResultCallback& callback,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const ClientIncidentReport& report);

 private:
  IncidentReportUploaderImpl(
      const OnResultCallback& callback,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const std::string& post_data);

  // Returns the URL to which incident reports are to be sent.
  static GURL GetIncidentReportUrl();

  // net::URLFetcherDelegate methods.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // The time at which the upload was initiated.
  base::TimeTicks time_begin_;

  DISALLOW_COPY_AND_ASSIGN(IncidentReportUploaderImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORT_UPLOADER_IMPL_H_
