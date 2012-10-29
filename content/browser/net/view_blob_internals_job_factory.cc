// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/view_blob_internals_job_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "content/public/common/url_constants.h"
#include "webkit/blob/view_blob_internals_job.h"

namespace content {

// static.
bool ViewBlobInternalsJobFactory::IsSupportedURL(const GURL& url) {
  return url.SchemeIs(chrome::kChromeUIScheme) &&
         url.host() == chrome::kChromeUIBlobInternalsHost;
}

// static.
net::URLRequestJob* ViewBlobInternalsJobFactory::CreateJobForRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    webkit_blob::BlobStorageController* blob_storage_controller) {
  return new webkit_blob::ViewBlobInternalsJob(
      request, network_delegate, blob_storage_controller);
}

}  // namespace content
