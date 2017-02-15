// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "content/browser/background_fetch/fetch_request.h"
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
  void CreateRequest(const FetchRequest& fetch_request);

 private:
  // BackgroundFetchContext owns this BackgroundFetchDataManager, so the
  // DataManager is guaranteed to be destructed before the Context.
  BackgroundFetchContext* background_fetch_context_;

  // Map from <sw_registration_id, tag> to the FetchRequest for that tag.
  using FetchIdentifier = std::pair<int64_t, std::string>;
  std::map<FetchIdentifier, FetchRequest> fetch_map_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
