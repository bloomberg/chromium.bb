// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
#define CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_simple_job.h"
#include "url/gurl.h"

namespace content {

class HistogramInternalsRequestJob : public net::URLRequestSimpleJob {
 public:
  HistogramInternalsRequestJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate);

  // net::URLRequestSimpleJob:
  void Start() override;
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override;

  // Generates the HTML for chrome://histograms. If |url| has a path, it's used
  // to display a single histogram.
  // base::StatisticsRecorder::ImportProvidedHistograms must have been called
  // on the UI thread first.
  static std::string GenerateHTML(const GURL& url);

 private:
  ~HistogramInternalsRequestJob() override;

  // Starts the real URL request.
  void StartUrlRequest();

  // The URL that was requested, which might have a path component to a specific
  // histogram.
  GURL url_;

  base::WeakPtrFactory<HistogramInternalsRequestJob> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HistogramInternalsRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
