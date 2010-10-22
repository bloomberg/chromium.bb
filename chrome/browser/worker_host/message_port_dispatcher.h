// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_MESSAGE_PORT_DISPATCHER_H_
#define CHROME_BROWSER_WORKER_HOST_MESSAGE_PORT_DISPATCHER_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/task.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "ipc/ipc_message.h"

class MessagePortDispatcher : public NotificationObserver {
 public:
  typedef std::vector<std::pair<string16, std::vector<int> > > QueuedMessages;

  // Returns the MessagePortDispatcher singleton.
  static MessagePortDispatcher* GetInstance();

  bool OnMessageReceived(const IPC::Message& message,
                         IPC::Message::Sender* sender,
                         CallbackWithReturnValue<int>::Type* next_routing_id,
                         bool* message_was_ok);

  // Updates the information needed to reach a message port when it's sent to a
  // (possibly different) process.
  void UpdateMessagePort(
      int message_port_id,
      IPC::Message::Sender* sender,
      int routing_id,
      CallbackWithReturnValue<int>::Type* next_routing_id);

  // Attempts to send the queued messages for a message port.
  void SendQueuedMessagesIfPossible(int message_port_id);

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
                     const std::vector<int>& sent_message_port_ids);
  void OnQueueMessages(int message_port_id);
  void OnSendQueuedMessages(int message_port_id,
                            const QueuedMessages& queued_messages);

  void PostMessageTo(int message_port_id,
                     const string16& message,
                     const std::vector<int>& sent_message_port_ids);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Handles the details of removing a message port id. Before calling this,
  // verify that the message port id exists.
  void Erase(int message_port_id);

  struct MessagePort;
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
