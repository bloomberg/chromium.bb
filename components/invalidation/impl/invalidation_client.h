// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_CLIENT_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_CLIENT_H_

namespace syncer {

// The client handler all startup/shutdown behaviour.
class InvalidationClient {
 public:
  virtual ~InvalidationClient() {}

  /* Starts the client. This method must be called before any other method is
   * invoked. The client is considered to be started after
   * InvalidationListener::Ready has received by the application.
   *
   * REQUIRES: Start has not already been called.
   * The resources given to the client must have been started by the caller.
   */
  virtual void Start() = 0;

  /* Stops the client. After this method has been called, it is an error to call
   * any other method.
   *
   * REQUIRES: Start has already been called.
   * Does not stop the resources bound to this client.
   */
  virtual void Stop() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_CLIENT_H_
