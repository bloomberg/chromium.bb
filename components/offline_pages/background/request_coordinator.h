// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_

#include "base/callback.h"

namespace offline_pages {

// Coordinates queueing and processing save page later requests.
class RequestCoordinator {
 public:
  // Callback to report when the processing of a triggered task is complete.
  typedef base::Callback<void()> ProcessingDoneCallback;

  struct SavePageRequest {
    // TODO(dougarnett): define and consider making stand-alone.
  };

  // TODO(dougarnett): How to inject Offliner factories and policy objects.
  RequestCoordinator();

  ~RequestCoordinator();

  // Queues |request| to later load and save when system conditions allow.
  bool SavePageLater(const SavePageRequest& request);

  // Starts processing of one or more queued save page later requests.
  // Returns whether processing was started and that caller should expect
  // a callback. If processing was already active, returns false.
  bool StartProcessing(const ProcessingDoneCallback& callback);

  // Stops the current request processing if active. This is a way for
  // caller to abort processing; otherwise, processing will complete on
  // its own. In either case, the callback will be called when processing
  // is stopped or complete.
  void StopProcessing();

  // TODO(dougarnett): add policy support methods.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
