// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_FETCH_REQUEST_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_FETCH_REQUEST_H_

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Simple class to encapsulate the components of a fetch request.
class CONTENT_EXPORT FetchRequest {
 public:
  FetchRequest();
  FetchRequest(const url::Origin& origin,
               const GURL& url,
               int64_t sw_registration_id,
               const std::string& tag);
  // TODO(harkness): Remove copy constructor once the final (non-map-based)
  // state management is in place.
  FetchRequest(const FetchRequest& request);
  ~FetchRequest();

  const std::string& guid() const { return guid_; }
  const url::Origin& origin() const { return origin_; }
  const GURL& url() const { return url_; }
  int64_t service_worker_registration_id() const {
    return service_worker_registration_id_;
  }
  const std::string& tag() const { return tag_; }

 private:
  std::string guid_;
  url::Origin origin_;
  GURL url_;
  int64_t service_worker_registration_id_ = kInvalidServiceWorkerRegistrationId;
  std::string tag_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_FETCH_REQUEST_H_
