// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

class RequestQueueStore;

// Class responsible for managing save page requests.
class RequestQueue {
 public:
  enum class GetRequestsResult {
    SUCCESS,
    STORE_FAILURE,
  };

  enum class AddRequestResult {
    SUCCESS,
    STORE_FAILURE,
    REQUEST_QUOTA_HIT,  // Cannot add a request with this namespace, as it has
                        // reached a quota of active requests.
  };

  // GENERATED_JAVA_ENUM_PACKAGE:org.chromium.components.offlinepages.background
  enum class UpdateRequestResult {
    SUCCESS,
    STORE_FAILURE,
    REQUEST_DOES_NOT_EXIST,  // Failed to delete the request because it does not
                             // exist.
  };

  // Type for a pair of request_id and result.
  typedef std::vector<std::pair<int64_t, UpdateRequestResult>>
      UpdateMultipleRequestResults;

  // Callback used for |GetRequests|.
  typedef base::Callback<void(GetRequestsResult,
                              const std::vector<SavePageRequest>&)>
      GetRequestsCallback;

  // Callback used for |AddRequest|.
  typedef base::Callback<void(AddRequestResult, const SavePageRequest& request)>
      AddRequestCallback;

  // Callback used by |UdpateRequest|.
  typedef base::Callback<void(UpdateRequestResult)> UpdateRequestCallback;

  // Callback used by |ChangeState| for more than one update at a time.
  typedef base::Callback<void(const UpdateMultipleRequestResults& results,
                              const std::vector<SavePageRequest>& requests)>
      UpdateMultipleRequestsCallback;

  // Callback used by |RemoveRequests|.
  typedef base::Callback<void(const UpdateMultipleRequestResults& results,
                              const std::vector<SavePageRequest>& requests)>
      RemoveRequestsCallback;

  explicit RequestQueue(std::unique_ptr<RequestQueueStore> store);
  ~RequestQueue();

  // Gets all of the active requests from the store. Calling this method may
  // schedule purging of the request queue.
  void GetRequests(const GetRequestsCallback& callback);

  // Adds |request| to the request queue. Result is returned through |callback|.
  // In case adding the request violates policy, the result will fail with
  // appropriate result. Callback will also return a copy of a request with all
  // fields set.
  void AddRequest(const SavePageRequest& request,
                  const AddRequestCallback& callback);

  // Updates a request in the request queue if a request with matching ID
  // exists. Does nothing otherwise. Result is returned through |callback|.
  void UpdateRequest(const SavePageRequest& request,
                     const UpdateRequestCallback& callback);

  // Removes the requests matching the |request_ids|. Result is returned through
  // |callback|.  If a request id cannot be removed, this will still remove the
  // others.
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      const RemoveRequestsCallback& callback);

  // Changes the state to |new_state_ for requests matching the
  // |request_ids|. Results are returned through |callback|.
  // TODO(petewil): Instead of having one function per property,
  // modify this to have a single update function that updates an entire
  // request, and doesn't need to care what updates.
  void ChangeRequestsState(const std::vector<int64_t>& request_ids,
                           const SavePageRequest::RequestState new_state,
                           const UpdateMultipleRequestsCallback& callback);

  void GetForUpdateDone(
      const RequestQueue::UpdateRequestCallback& update_callback,
      const SavePageRequest& update_request,
      bool success,
      const std::vector<SavePageRequest>& requests);

 private:
  // Callback used by |PurgeRequests|.
  typedef base::Callback<void(UpdateRequestResult,
                              int /* removed requests count */)>
      PurgeRequestsCallback;

  // Purges the queue, removing the requests that are no longer relevant, e.g.
  // expired request. Result is returned through |callback| carries the number
  // of removed requests.
  void PurgeRequests(const PurgeRequestsCallback& callback);

  std::unique_ptr<RequestQueueStore> store_;

  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestQueue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RequestQueue);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_
