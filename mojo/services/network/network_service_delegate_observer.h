// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_OBSERVER_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_OBSERVER_H_

namespace mojo {

// Observer of events on from the NetworkServiceDelegate.
//
// Observers should be registered on the main thread.
class NetworkServiceDelegateObserver {
 public:
  ~NetworkServiceDelegateObserver() {}

  // Broadcast right before we attempt to shutdown the IO worker thread. This
  // is
  virtual void OnIOWorkerThreadShutdown() = 0;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_OBSERVER_H_
