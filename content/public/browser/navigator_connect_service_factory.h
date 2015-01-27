// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_

#include "base/callback.h"

class GURL;

namespace content {

struct NavigatorConnectClient;
class MessagePortDelegate;

// Implement this interface to provide a new kind of navigator.connect
// accessible service.
// Instances of this class are owned by NavigatorConnectContext. Register new
// factories by calling NavigatorConnectContext::AddFactory.
class NavigatorConnectServiceFactory {
 public:
  // Call with nullptr to indicate connection failure. Ownership of the delegate
  // remains with the factory. It is assumed that for the passed
  // MessagePortDelegate implementation the route_id and message_port_id of a
  // connection are the same.
  using ConnectCallback = base::Callback<void(MessagePortDelegate*)>;

  virtual ~NavigatorConnectServiceFactory() {}

  // Return true if this factory is responsible for handling connections to the
  // |target_url|. The most recently added factory that returns true for a
  // particular url is picked to handle the connection attempt.
  virtual bool HandlesUrl(const GURL& target_url) = 0;

  // Called to try to establish a connection. Only called if this factory was
  // the most recently added factory that returned true from |HandlesUrl| for
  // the url being connected to.
  virtual void Connect(const NavigatorConnectClient& client,
                       const ConnectCallback& callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_
