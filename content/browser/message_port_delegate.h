// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MESSAGE_PORT_DELEGATE_H_
#define CONTENT_BROWSER_MESSAGE_PORT_DELEGATE_H_

#include <vector>

#include "base/strings/string16.h"

namespace content {

// Delegate used by MessagePortService to send messages to message ports to the
// correct renderer. Delegates are responsible for managing their own lifetime,
// and should call MessagePortService::OnMessagePortDelegateClosing if they are
// destroyed while there are still message ports associated with them.
class MessagePortDelegate {
 public:
  // Sends a message to the given route. Implementations are responsible for
  // updating MessagePortService with new routes for the sent message ports.
  virtual void SendMessage(int route_id,
                           const base::string16& message,
                           const std::vector<int>& sent_message_port_ids) = 0;

  // Sends a "messages are queued" IPC to the given route.
  virtual void SendMessagesAreQueued(int route_id) = 0;

 protected:
  virtual ~MessagePortDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_MESSAGE_PORT_DELEGATE_H_
