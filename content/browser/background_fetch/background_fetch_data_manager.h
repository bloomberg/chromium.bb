// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "content/browser/background_fetch/background_fetch_job_data.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class BackgroundFetchContext;

// The BackgroundFetchDataManager keeps track of all of the outstanding requests
// which are in process in the DownloadManager. When Chromium restarts, it is
// responsibile for reconnecting all the in progress downloads with an observer
// which will keep the metadata up to date.
class CONTENT_EXPORT BackgroundFetchDataManager {
 public:
  explicit BackgroundFetchDataManager(
      BackgroundFetchContext* background_fetch_context);
  ~BackgroundFetchDataManager();

  // Called by BackgroundFetchContext when a new request is started, this will
  // store all of the necessary metadata to track the request.
  std::unique_ptr<BackgroundFetchJobData> CreateRequest(
      std::unique_ptr<BackgroundFetchJobInfo> job_info,
      BackgroundFetchRequestInfos request_infos);

 private:
  void WriteJobToStorage(std::unique_ptr<BackgroundFetchJobInfo> job_info,
                         BackgroundFetchRequestInfos request_infos);

  BackgroundFetchRequestInfos& ReadRequestsFromStorage(
      const std::string& job_guid);

  // BackgroundFetchContext owns this BackgroundFetchDataManager, so the
  // DataManager is guaranteed to be destructed before the Context.
  BackgroundFetchContext* background_fetch_context_;

  // Map from <sw_registration_id, tag> to the job_guid for that tag.
  using JobIdentifier = std::pair<int64_t, std::string>;
  std::map<JobIdentifier, std::string> service_worker_tag_map_;

  // Temporary map to hold data which will be written to storage.
  // Map from job_guid to JobInfo.
  std::unordered_map<std::string, std::unique_ptr<BackgroundFetchJobInfo>>
      job_map_;
  // Map from job_guid to RequestInfos.
  std::unordered_map<std::string, BackgroundFetchRequestInfos> request_map_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
