// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_DATA_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_DATA_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// The BackgroundFetchJobData class provides the interface from the
// JobController to the data storage in the DataManager. It has a reference
// to the DataManager and invokes calls given the stored batch_guid.
class CONTENT_EXPORT BackgroundFetchJobData {
 public:
  explicit BackgroundFetchJobData(BackgroundFetchRequestInfos request_infos);
  ~BackgroundFetchJobData();

  // Called by the JobController to inform the JobData that the given fetch
  // has completed. The JobData returns a boolean indicating whether there
  // are more requests to process.
  bool BackgroundFetchRequestInfoComplete(const std::string& fetch_guid);

  // Called by the JobController to get a BackgroundFetchRequestInfo to
  // process.
  const BackgroundFetchRequestInfo& GetNextBackgroundFetchRequestInfo();

  // Indicates whether all requests have been handed to the JobController.
  bool HasRequestsRemaining() const;

  // Indicates whether all requests have been handed out and completed.
  bool IsComplete() const;

 private:
  BackgroundFetchRequestInfos request_infos_;

  // Map from fetch_guid to index in request_infos_. Only currently
  // outstanding requests should be in this map.
  std::unordered_map<std::string, int> request_info_index_;
  size_t next_request_info_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobData);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_DATA_H_
