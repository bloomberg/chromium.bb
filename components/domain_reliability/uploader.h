// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_

#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequest;
class URLRequestContextGetter;
}  // namespace net

namespace domain_reliability {

class DOMAIN_RELIABILITY_EXPORT DomainReliabilityUploader {
 public:
  typedef base::Callback<void(bool success)> UploadCallback;

  DomainReliabilityUploader();
  virtual ~DomainReliabilityUploader();

  static scoped_ptr<DomainReliabilityUploader> Create(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  virtual void UploadReport(const std::string& report_json,
                            const GURL& upload_url,
                            const UploadCallback& callback) = 0;

  static bool URLRequestIsUpload(const net::URLRequest& request);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
