// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/view_blob_internals_job_factory.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "net/url_request/url_request.h"
#include "webkit/blob/view_blob_internals_job.h"

// static.
bool ViewBlobInternalsJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(),
                         chrome::kBlobViewInternalsURL,
                         true /*case_sensitive*/);
}

// static.
net::URLRequestJob* ViewBlobInternalsJobFactory::CreateJobForRequest(
    net::URLRequest* request) {
  webkit_blob::BlobStorageController* blob_storage_controller =
      static_cast<ChromeURLRequestContext*>(request->context())->
          blob_storage_context()->controller();
  return new webkit_blob::ViewBlobInternalsJob(
      request, blob_storage_controller);
}
