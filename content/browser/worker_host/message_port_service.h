// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_
#define CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/task.h"
#include "ipc/ipc_message.h"

class WorkerMessageFilter;

class MessagePortService {
 public:
  typedef std::vector<std::pair<string16, std::vector<int> > > QueuedMessages;

  // Returns the MessagePortService singleton.
  static MessagePortService* GetInstance();

  // These methods correspond to the message port related IPCs.
  void Create(int route_id, WorkerMessageFilter* filter, int* message_port_id);
  void Destroy(int message_port_id);
  void Entangle(int local_message_port_id, int remote_message_port_id);
  void PostMessage(int sender_message_port_id,
                   const string16& message,
                   const std::vector<int>& sent_message_port_ids);
  void QueueMessages(int message_port_id);
  void SendQueuedMessages(int message_port_id,
                          const QueuedMessages& queued_messages);

  // Updates the information needed to reach a message port when it's sent to a
  // (possibly different) process.
  void UpdateMessagePort(
      int message_port_id,
      WorkerMessageFilter* filter,
      int routing_id);

  void OnWorkerMessageFilterClosing(WorkerMessageFilter* filter);

  // Attempts to send the queued messages for a message port.
  void SendQueuedMessagesIfPossible(int message_port_id);

 private:
  friend struct DefaultSingletonTraits<MessagePortService>;

  MessagePortService();
  ~MessagePortService();

  void PostMessageTo(int message_port_id,
                     const string16& message,
                     const std::vector<int>& sent_message_port_ids);

  // Handles the details of removing a message port id. Before calling this,
  // verify that the message port id exists.
  void Erase(int message_port_id);

  struct MessagePort;
  typedef std::map<int, MessagePort> MessagePorts;
  MessagePorts message_ports_;

  // We need globally unique identifiers for each message port.
  int next_message_port_id_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortService);
};

#endif  // CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_
