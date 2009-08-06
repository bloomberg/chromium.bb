// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_MESSAGE_PORT_DISPATCHER_H_
#define CHROME_BROWSER_WORKER_HOST_MESSAGE_PORT_DISPATCHER_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/task.h"
#include "chrome/common/notification_registrar.h"
#include "ipc/ipc_message.h"

class MessagePortDispatcher : public NotificationObserver {
 public:
  typedef std::vector<std::pair<string16, int> > QueuedMessages;

  // Returns the MessagePortDispatcher singleton.
  static MessagePortDispatcher* GetInstance();

  bool OnMessageReceived(const IPC::Message& message,
                         IPC::Message::Sender* sender,
                         CallbackWithReturnValue<int>::Type* next_routing_id,
                         bool* message_was_ok);

  void UpdateMessagePort(
      int message_port_id,
      IPC::Message::Sender* sender,
      int routing_id,
      CallbackWithReturnValue<int>::Type* next_routing_id);

  bool Send(IPC::Message* message);

 private:
  friend struct DefaultSingletonTraits<MessagePortDispatcher>;

  MessagePortDispatcher();
  ~MessagePortDispatcher();

  // Message handlers.
  void OnCreate(int* route_id, int* message_port_id);
  void OnDestroy(int message_port_id);
  void OnEntangle(int local_message_port_id, int remote_message_port_id);
  void OnPostMessage(int sender_message_port_id,
                     const string16& message,
                     int sent_message_port_id);
  void OnQueueMessages(int message_port_id);
  void OnSendQueuedMessages(int message_port_id,
                            const QueuedMessages& queued_messages);

  void PostMessageTo(int message_port_id,
                     const string16& message,
                     int sent_message_port_id);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  struct MessagePort {
    // sender and route_id are what we need to send messages to the port.
    IPC::Message::Sender* sender;
    int route_id;
    // A function pointer to generate a new route id for the sender above.
    // Owned by "sender" above, so don't delete.
    CallbackWithReturnValue<int>::Type* next_routing_id;
    // A globally unique id for this message port.
    int message_port_id;
    // The globally unique id of the entangled message port.
    int entangled_message_port_id;
    // If true, all messages to this message port are queued and not delivered.
    bool queue_messages;
    QueuedMessages queued_messages;
  };

  typedef std::map<int, MessagePort> MessagePorts;
  MessagePorts message_ports_;

  // We need globally unique identifiers for each message port.
  int next_message_port_id_;

  // Valid only during IPC message dispatching.
  IPC::Message::Sender* sender_;
  CallbackWithReturnValue<int>::Type* next_routing_id_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortDispatcher);
};

#endif  // CHROME_BROWSER_WORKER_HOST_MESSAGE_PORT_DISPATCHER_H_
