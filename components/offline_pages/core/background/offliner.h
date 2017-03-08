// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_

#include <string>

#include "base/callback.h"

namespace offline_pages {

class SavePageRequest;

// Interface of a class responsible for constructing an offline page given
// a request with a URL.
class Offliner {
 public:
  // Status of processing an offline page request.
  // WARNING: You must update histograms.xml to match any changes made to
  // this enum (ie, OfflinePagesBackgroundOfflinerRequestStatus histogram enum).
  // Also update related switch code in RequestCoordinatorEventLogger.
  enum RequestStatus {
    // No status determined/reported yet. Interim status, not sent in callback.
    UNKNOWN = 0,
    // Page loaded but not (yet) saved. Interim status, not sent in callback.
    LOADED = 1,
    // Offline page snapshot saved.
    SAVED = 2,
    // RequestCoordinator canceled request.
    REQUEST_COORDINATOR_CANCELED = 3,
    // Loading was canceled.
    LOADING_CANCELED = 4,
    // Loader failed to load page.
    LOADING_FAILED = 5,
    // Failed to save loaded page.
    SAVE_FAILED = 6,
    // Foreground transition canceled request.
    FOREGROUND_CANCELED = 7,
    // RequestCoordinator canceled request attempt per time limit.
    REQUEST_COORDINATOR_TIMED_OUT = 8,
    // Deprecated. The RequestCoordinator did not start loading the request.
    DEPRECATED_LOADING_NOT_STARTED = 9,
    // Loader failed with hard error so should not retry the request.
    LOADING_FAILED_NO_RETRY = 10,
    // Loader failed with some error that suggests we should not continue
    // processing another request at this time.
    LOADING_FAILED_NO_NEXT = 11,
    // The RequestCoordinator tried to start loading the request but the
    // loading request was not accepted.
    LOADING_NOT_ACCEPTED = 12,
    // The RequestCoordinator did not start loading the request because
    // updating the status in the request queue failed.
    QUEUE_UPDATE_FAILED = 13,
    // Scheduler canceled processing of requests.
    BACKGROUND_SCHEDULER_CANCELED = 14,
    // NOTE: insert new values above this line and update histogram enum too.
    STATUS_COUNT
  };

  // Reports the load progress of a request.
  typedef base::Callback<void(const SavePageRequest&, int64_t received_bytes)>
      ProgressCallback;
  // Reports the completion status of a request.
  // TODO(dougarnett): consider passing back a request id instead of request.
  typedef base::Callback<void(const SavePageRequest&, RequestStatus)>
      CompletionCallback;
  // Reports that the cancel operation has completed.
  // TODO(chili): make save operation cancellable.
  typedef base::Callback<void(int64_t request_id)> CancelCallback;

  Offliner() {}
  virtual ~Offliner() {}

  // Processes |request| to load and save an offline page.
  // Returns whether the request was accepted or not. |completion_callback| is
  // guaranteed to be called if the request was accepted and |Cancel()| is not
  // called on it. |progress_callback| is invoked periodically to report the
  // number of bytes received from the network (for UI purposes).
  virtual bool LoadAndSave(const SavePageRequest& request,
                           const CompletionCallback& completion_callback,
                           const ProgressCallback& progress_callback) = 0;

  // Clears the currently processing request, if any, and skips running its
  // CompletionCallback.
  virtual void Cancel(const CancelCallback& callback) = 0;

  // Handles timeout scenario. Returns true if lowbar is met and try to do a
  // snapshot of the current webcontents.
  virtual bool HandleTimeout(const SavePageRequest& request) = 0;

  // TODO(dougarnett): add policy support methods.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_
