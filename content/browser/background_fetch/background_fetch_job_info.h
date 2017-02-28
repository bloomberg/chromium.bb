// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/origin.h"

namespace content {

// Class to encapsulate a batch of FetchRequests into a single grouping which is
// what the developer requested.
class CONTENT_EXPORT BackgroundFetchJobInfo {
 public:
  BackgroundFetchJobInfo(const std::string& tag,
                         const url::Origin& origin,
                         int64_t service_worker_registration_id);
  ~BackgroundFetchJobInfo();

  const std::string& guid() const { return guid_; }
  const std::string& tag() const { return tag_; }
  const url::Origin& origin() const { return origin_; }
  int64_t service_worker_registration_id() const {
    return service_worker_registration_id_;
  }

  const std::vector<std::string>& request_guids() const {
    return request_guids_;
  }

 private:
  std::string guid_;
  std::string tag_;
  url::Origin origin_;
  int64_t service_worker_registration_id_ = kInvalidServiceWorkerRegistrationId;
  std::vector<std::string> request_guids_;

  // TODO(harkness): Other things this class should track: estimated download
  // size, current download size, total number of files, number of complete
  // files, (possibly) data to show the notification, (possibly) list of in
  // progress FetchRequests.

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H
