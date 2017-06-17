// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {
class PrefetchService;

// Serves as the entry point for external signals into the prefetching system.
// It listens to these events, converts them to the appropriate internal tasks
// and manage their execution and inter-dependencies.
class PrefetchDispatcher {
 public:
  // A |ScopedBackgroundTask| is created when we are running in a background
  // task.  Destroying this object should notify the system that we are done
  // processing the background task.
  class ScopedBackgroundTask {
   public:
    ScopedBackgroundTask() = default;
    virtual ~ScopedBackgroundTask() = default;

    // Used on destruction to inform the system about whether rescheduling with
    // or without backoff is required.
    virtual void SetNeedsReschedule(bool reschedule, bool backoff) = 0;
  };

  virtual ~PrefetchDispatcher() = default;

  // Initializes the dispatcher with its respective service instance. This must
  // be done before any other methods are called.
  virtual void SetService(PrefetchService* service) = 0;

  // Called when a client has candidate URLs for the system to prefetch, along
  // with the client's unique namespace. URLs that are currently in the system
  // for this client are acceptable but ignored.
  virtual void AddCandidatePrefetchURLs(
      const std::string& name_space,
      const std::vector<PrefetchURL>& prefetch_urls) = 0;

  // Called when all existing suggestions are no longer considered valid for a
  // given namespace.  The prefetch system should remove any URLs that
  // have not yet started downloading within that namespace.
  virtual void RemoveAllUnprocessedPrefetchURLs(
      const std::string& name_space) = 0;

  // Called to invalidate a single PrefetchURL entry identified by |client_id|.
  // If multiple have the same |client_id|, they will all be removed.
  virtual void RemovePrefetchURLsByClientId(const ClientId& client_id) = 0;

  // Called when Android OS has scheduled us for background work.  When
  // destroyed, |task| will call back and inform the OS that we are done work
  // (if required).  |task| also manages rescheduling behavior.
  virtual void BeginBackgroundTask(
      std::unique_ptr<ScopedBackgroundTask> task) = 0;

  // Called when a task must stop immediately due to system constraints. After
  // this call completes, the system will reschedule the task based on whether
  // SetNeedsReschedule has been called.
  virtual void StopBackgroundTask() = 0;

  // Called when the GCM app handler receives a GCM message with an embeddeed
  // operation name.
  virtual void GCMOperationCompletedMessageReceived(
      const std::string& operation_name) = 0;

  // Used by the test to signal the completion of the background task.
  virtual void RequestFinishBackgroundTaskForTest() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_H_
