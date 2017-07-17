// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_

namespace mojo {

class Message;

// A control interface exposed by AssociatedGroupController for interface
// endpoints.
class InterfaceEndpointController {
 public:
  virtual ~InterfaceEndpointController() {}

  virtual bool SendMessage(Message* message) = 0;

  // Allows the interface endpoint to watch for incoming sync messages while
  // others perform sync handle watching on the same sequence. Please see
  // comments of SyncHandleWatcher::AllowWokenUpBySyncWatchOnSameThread().
  virtual void AllowWokenUpBySyncWatchOnSameThread() = 0;

  // Watches the interface endpoint for incoming sync messages. (It also watches
  // other other handles registered to be watched together.)
  // This method:
  //   - returns true when |should_stop| is set to true;
  //   - return false otherwise, including
  //     MultiplexRouter::DetachEndpointClient() being called for the same
  //     interface endpoint.
  virtual bool SyncWatch(const bool* should_stop) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
