// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator_event_logger.h"

namespace offline_pages {

namespace {

static std::string OfflinerRequestStatusToString(
    Offliner::RequestStatus request_status) {
  switch (request_status) {
    case Offliner::UNKNOWN:
      return "UNKNOWN";
    case Offliner::LOADED:
      return "LOADED";
    case Offliner::SAVED:
      return "SAVED";
    case Offliner::REQUEST_COORDINATOR_CANCELED:
      return "REQUEST_COORDINATOR_CANCELED";
    case Offliner::PRERENDERING_CANCELED:
      return "PRERENDERING_CANCELED";
    case Offliner::PRERENDERING_FAILED:
      return "PRERENDERING_FAILED";
    case Offliner::SAVE_FAILED:
      return "SAVE_FAILED";
    case Offliner::FOREGROUND_CANCELED:
      return "FOREGROUND_CANCELED";
    default:
      DCHECK(false);
      return "";
  }
}

static std::string UpdateRequestResultToString(
    RequestQueue::UpdateRequestResult result) {
  switch (result) {
    case RequestQueue::UpdateRequestResult::SUCCESS:
      return "SUCCESS";
    case RequestQueue::UpdateRequestResult::STORE_FAILURE:
      return "STORE_FAILURE";
    case RequestQueue::UpdateRequestResult::REQUEST_DOES_NOT_EXIST:
      return "REQUEST_DOES_NOT_EXIST";
    default:
      DCHECK(false);
      return "";
  }
}

}  // namespace

void RequestCoordinatorEventLogger::RecordSavePageRequestUpdated(
    const std::string& name_space,
    Offliner::RequestStatus new_status,
    int64_t id) {
  RecordActivity("Save page request for ID: " + std::to_string(id) +
                 " and namespace: " + name_space +
                 " has been updated with status " +
                 OfflinerRequestStatusToString(new_status));
}

void RequestCoordinatorEventLogger::RecordUpdateRequestFailed(
    const std::string& name_space,
    RequestQueue::UpdateRequestResult result) {
  RecordActivity("Updating queued request for namespace: " + name_space +
                 " failed with result: " + UpdateRequestResultToString(result));
}

}  // namespace offline_pages
