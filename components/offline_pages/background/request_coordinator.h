// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace offline_pages {

class OfflinerPolicy;
class OfflinerFactory;
class Offliner;
class SavePageRequest;

// Coordinates queueing and processing save page later requests.
class RequestCoordinator : public KeyedService {
 public:
  // Callback to report when the processing of a triggered task is complete.
  typedef base::Callback<void()> ProcessingDoneCallback;

  RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                     std::unique_ptr<OfflinerFactory> factory);

  ~RequestCoordinator() override;

  // Queues |request| to later load and save when system conditions allow.
  // Returns true if the page could be queued successfully.
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

 private:
  // RequestCoordinator takes over ownership of the policy
  std::unique_ptr<OfflinerPolicy> policy_;
  // Factory is owned by the RequestCoordinator.
  std::unique_ptr<OfflinerFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(RequestCoordinator);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
