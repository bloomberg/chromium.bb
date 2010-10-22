// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/message_port_dispatcher.h"

#include "base/callback.h"
#include "base/singleton.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/worker_messages.h"

struct MessagePortDispatcher::MessagePort {
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

MessagePortDispatcher* MessagePortDispatcher::GetInstance() {
  return Singleton<MessagePortDispatcher>::get();
}

MessagePortDispatcher::MessagePortDispatcher()
    : next_message_port_id_(0),
      sender_(NULL),
      next_routing_id_(NULL) {
  // Receive a notification if a message filter or WorkerProcessHost is deleted.
  registrar_.Add(this, NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
                 NotificationService::AllSources());

  registrar_.Add(this, NotificationType::WORKER_PROCESS_HOST_SHUTDOWN,
                 NotificationService::AllSources());
}

MessagePortDispatcher::~MessagePortDispatcher() {
}

bool MessagePortDispatcher::OnMessageReceived(
    const IPC::Message& message,
    IPC::Message::Sender* sender,
    CallbackWithReturnValue<int>::Type* next_routing_id,
    bool* message_was_ok) {
  sender_ = sender;
  next_routing_id_ = next_routing_id;

  bool handled = true;
  *message_was_ok = true;

  IPC_BEGIN_MESSAGE_MAP_EX(MessagePortDispatcher, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_CreateMessagePort, OnCreate)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_DestroyMessagePort, OnDestroy)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_Entangle, OnEntangle)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_QueueMessages, OnQueueMessages)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_SendQueuedMessages,
                        OnSendQueuedMessages)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  sender_ = NULL;
  next_routing_id_ = NULL;

  return handled;
}

void MessagePortDispatcher::UpdateMessagePort(
    int message_port_id,
    IPC::Message::Sender* sender,
    int routing_id,
    CallbackWithReturnValue<int>::Type* next_routing_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  port.sender = sender;
  port.route_id = routing_id;
  port.next_routing_id = next_routing_id;
}

bool MessagePortDispatcher::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void MessagePortDispatcher::OnCreate(int *route_id,
                                     int* message_port_id) {
  *message_port_id = ++next_message_port_id_;
  *route_id = next_routing_id_->Run();

  MessagePort port;
  port.sender = sender_;
  port.route_id = *route_id;
  port.next_routing_id = next_routing_id_;
  port.message_port_id = *message_port_id;
  port.entangled_message_port_id = MSG_ROUTING_NONE;
  port.queue_messages = false;
  message_ports_[*message_port_id] = port;
}

void MessagePortDispatcher::OnDestroy(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  DCHECK(message_ports_[message_port_id].queued_messages.empty());
  Erase(message_port_id);
}

void MessagePortDispatcher::OnEntangle(int local_message_port_id,
                                       int remote_message_port_id) {
  if (!message_ports_.count(local_message_port_id) ||
      !message_ports_.count(remote_message_port_id)) {
    NOTREACHED();
    return;
  }

  DCHECK(message_ports_[remote_message_port_id].entangled_message_port_id ==
      MSG_ROUTING_NONE);
  message_ports_[remote_message_port_id].entangled_message_port_id =
      local_message_port_id;
}

void MessagePortDispatcher::OnPostMessage(
    int sender_message_port_id,
    const string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!message_ports_.count(sender_message_port_id)) {
    NOTREACHED();
    return;
  }

  int entangled_message_port_id =
      message_ports_[sender_message_port_id].entangled_message_port_id;
  if (entangled_message_port_id == MSG_ROUTING_NONE)
    return;  // Process could have crashed.

  if (!message_ports_.count(entangled_message_port_id)) {
    NOTREACHED();
    return;
  }

  PostMessageTo(entangled_message_port_id, message, sent_message_port_ids);
}

void MessagePortDispatcher::PostMessageTo(
    int message_port_id,
    const string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    if (!message_ports_.count(sent_message_port_ids[i])) {
      NOTREACHED();
      return;
    }
  }

  MessagePort& entangled_port = message_ports_[message_port_id];

  std::vector<MessagePort*> sent_ports(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
    sent_ports[i] = &message_ports_[sent_message_port_ids[i]];
    sent_ports[i]->queue_messages = true;
  }

  if (entangled_port.queue_messages) {
    entangled_port.queued_messages.push_back(
        std::make_pair(message, sent_message_port_ids));
  } else {
    // If a message port was sent around, the new location will need a routing
    // id.  Instead of having the created port send us a sync message to get it,
    // send along with the message.
    std::vector<int> new_routing_ids(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      new_routing_ids[i] = entangled_port.next_routing_id->Run();
      sent_ports[i]->sender = entangled_port.sender;

      // Update the entry for the sent port as it can be in a different process.
      sent_ports[i]->route_id = new_routing_ids[i];
    }

    // Now send the message to the entangled port.
    IPC::Message* ipc_msg = new WorkerProcessMsg_Message(
        entangled_port.route_id, message, sent_message_port_ids,
        new_routing_ids);
    entangled_port.sender->Send(ipc_msg);
  }
}

void MessagePortDispatcher::OnQueueMessages(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  port.sender->Send(new WorkerProcessMsg_MessagesQueued(port.route_id));
  port.queue_messages = true;
  port.sender = NULL;
}

void MessagePortDispatcher::OnSendQueuedMessages(
    int message_port_id,
    const QueuedMessages& queued_messages) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  // Send the queued messages to the port again.  This time they'll reach the
  // new location.
  MessagePort& port = message_ports_[message_port_id];
  port.queue_messages = false;
  port.queued_messages.insert(port.queued_messages.begin(),
                              queued_messages.begin(),
                              queued_messages.end());
  SendQueuedMessagesIfPossible(message_port_id);
}

void MessagePortDispatcher::SendQueuedMessagesIfPossible(int message_port_id) {
  if (!message_ports_.count(message_port_id)) {
    NOTREACHED();
    return;
  }

  MessagePort& port = message_ports_[message_port_id];
  if (port.queue_messages || !port.sender)
    return;

  for (QueuedMessages::iterator iter = port.queued_messages.begin();
       iter != port.queued_messages.end(); ++iter) {
    PostMessageTo(message_port_id, iter->first, iter->second);
  }
  port.queued_messages.clear();
}

void MessagePortDispatcher::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  IPC::Message::Sender* sender = NULL;
  if (type.value == NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN) {
    sender = Source<ResourceMessageFilter>(source).ptr();
  } else if (type.value == NotificationType::WORKER_PROCESS_HOST_SHUTDOWN) {
    sender = Source<WorkerProcessHost>(source).ptr();
  } else {
    NOTREACHED();
  }

  // Check if the (possibly) crashed process had any message ports.
  for (MessagePorts::iterator iter = message_ports_.begin();
       iter != message_ports_.end();) {
    MessagePorts::iterator cur_item = iter++;
    if (cur_item->second.sender == sender) {
      Erase(cur_item->first);
    }
  }
}

void MessagePortDispatcher::Erase(int message_port_id) {
  MessagePorts::iterator erase_item = message_ports_.find(message_port_id);
  DCHECK(erase_item != message_ports_.end());

  int entangled_id = erase_item->second.entangled_message_port_id;
  if (entangled_id != MSG_ROUTING_NONE) {
    // Do the disentanglement (and be paranoid about the other side existing
    // just in case something unusual happened during entanglement).
    if (message_ports_.count(entangled_id)) {
      message_ports_[entangled_id].entangled_message_port_id = MSG_ROUTING_NONE;
    }
  }
  message_ports_.erase(erase_item);
}
