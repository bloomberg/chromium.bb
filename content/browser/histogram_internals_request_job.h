// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
#define CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_simple_job.h"

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

 private:
  ~HistogramInternalsRequestJob() override;

  // Starts the real URL request.
  void StartUrlRequest();

  // The string to select histograms which have |path_| as a substring.
  std::string path_;

  base::WeakPtrFactory<HistogramInternalsRequestJob> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HistogramInternalsRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HISTOGRAM_INTERNALS_REQUEST_JOB_H_
