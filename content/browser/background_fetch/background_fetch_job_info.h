// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/origin.h"

namespace content {

class BackgroundFetchJobResponseData;
class BackgroundFetchRequestInfo;

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

  // The |job_response_data| holds information to build a response object to
  // return to the Mojo service.
  void set_job_response_data(
      std::unique_ptr<BackgroundFetchJobResponseData> job_response_data);
  BackgroundFetchJobResponseData* job_response_data() const {
    return job_response_data_.get();
  }

  // The following methods are used to track currently in progress requests.
  void set_num_requests(size_t num_requests) { num_requests_ = num_requests; }
  size_t num_requests() const { return num_requests_; }
  bool IsComplete() const;
  bool HasRequestsRemaining() const;
  size_t next_request_index() const { return next_request_index_; }

  void AddActiveRequest(std::unique_ptr<BackgroundFetchRequestInfo> request);
  BackgroundFetchRequestInfo* GetActiveRequest(
      const std::string& request_guid) const;
  void RemoveActiveRequest(const std::string& request_guid);

 private:
  std::string guid_;
  std::string tag_;
  url::Origin origin_;
  int64_t service_worker_registration_id_ = kInvalidServiceWorkerRegistrationId;
  std::vector<std::string> request_guids_;

  // Tracking information for currently executing requests.
  base::flat_set<std::unique_ptr<BackgroundFetchRequestInfo>> active_requests_;
  size_t num_requests_ = INT_MAX;
  size_t next_request_index_ = 0;

  // TODO(harkness): Other things this class should track: estimated download
  // size, current download size, total number of files, number of complete
  // files, (possibly) data to show the notification, (possibly) list of in
  // progress FetchRequests.

  // This value will be non-null only when the job is complete and the response
  // to the caller is being assembled.
  std::unique_ptr<BackgroundFetchJobResponseData> job_response_data_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_INFO_H
