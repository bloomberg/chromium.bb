// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_QUEUE_H_
#pragma once

#include <map>
#include <set>

#include "base/basictypes.h"

namespace net {
class URLRequest;
}  // namespace net

class ResourceDispatcherHostRequestInfo;
struct GlobalRequestID;

// Makes decisions about delaying or not each net::URLRequest in the queue.
// All methods are called on the IO thread.
class ResourceQueueDelegate {
 public:
  // Should return true if it wants the |request| to not be started at this
  // point. To start the delayed request, ResourceQueue::StartDelayedRequest
  // should be used.
  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id) = 0;

  // Called just before ResourceQueue shutdown. After that, the delegate
  // should not use the ResourceQueue.
  virtual void WillShutdownResourceQueue() = 0;

 protected:
  virtual ~ResourceQueueDelegate();
};

// Makes it easy to delay starting URL requests until specified conditions are
// met.
class ResourceQueue {
 public:
  typedef std::set<ResourceQueueDelegate*> DelegateSet;

  // UI THREAD ONLY ------------------------------------------------------------

  // Construct the queue. You must initialize it using Initialize.
  ResourceQueue();
  ~ResourceQueue();

  // Initialize the queue with set of delegates it should ask for each incoming
  // request.
  void Initialize(const DelegateSet& delegates);

  // IO THREAD ONLY ------------------------------------------------------------

  // Must be called before destroying the queue. No other methods can be called
  // after that.
  void Shutdown();

  // Takes care to start the |request| after all delegates allow that. If no
  // delegate demands delaying the request it will be started immediately.
  void AddRequest(net::URLRequest* request,
                  const ResourceDispatcherHostRequestInfo& request_info);

  // Tells the queue that the net::URLRequest object associated with
  // |request_id| is no longer valid.
  void RemoveRequest(const GlobalRequestID& request_id);

  // A delegate should call StartDelayedRequest when it wants to allow the
  // request to start. If it was the last delegate that demanded the request
  // to be delayed, the request will be started.
  void StartDelayedRequest(ResourceQueueDelegate* delegate,
                           const GlobalRequestID& request_id);

 private:
  typedef std::map<GlobalRequestID, net::URLRequest*> RequestMap;
  typedef std::map<GlobalRequestID, DelegateSet> InterestedDelegatesMap;

  // The registered delegates. Will not change after the queue has been
  // initialized.
  DelegateSet delegates_;

  // Stores net::URLRequest objects associated with each GlobalRequestID. This
  // helps decoupling the queue from ResourceDispatcherHost.
  RequestMap requests_;

  // Maps a GlobalRequestID to the set of delegates that want to prevent the
  // associated request from starting yet.
  InterestedDelegatesMap interested_delegates_;

  // True when we are shutting down.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(ResourceQueue);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_QUEUE_H_
