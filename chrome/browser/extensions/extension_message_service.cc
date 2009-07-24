// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/json_writer.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) ((port_id) / 2)
#define GET_CHANNEL_OPENER_ID(channel_id) ((channel_id) * 2)
#define GET_CHANNEL_RECEIVERS_ID(channel_id) ((channel_id) * 2 + 1)

// Port1 is always even, port2 is always odd.
#define IS_OPENER_PORT_ID(port_id) (((port_id) & 1) == 0)

// Change even to odd and vice versa, to get the other side of a given channel.
#define GET_OPPOSITE_PORT_ID(source_port_id) ((source_port_id) ^ 1)

struct ExtensionMessageService::MessagePort {
  IPC::Message::Sender* sender;
  int routing_id;

  MessagePort(IPC::Message::Sender* sender = NULL,
              int routing_id = MSG_ROUTING_CONTROL) :
     sender(sender), routing_id(routing_id) {}
};

struct ExtensionMessageService::MessageChannel {
  ExtensionMessageService::MessagePort opener;
  ExtensionMessageService::MessagePort receiver;
};


namespace {

static void DispatchOnConnect(const ExtensionMessageService::MessagePort& port,
                              int dest_port_id,
                              const std::string& channel_name,
                              const std::string& tab_json,
                              const std::string& extension_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(dest_port_id));
  args.Set(1, Value::CreateStringValue(channel_name));
  args.Set(2, Value::CreateStringValue(tab_json));
  args.Set(3, Value::CreateStringValue(extension_id));
  port.sender->Send(new ViewMsg_ExtensionMessageInvoke(
      port.routing_id, ExtensionMessageService::kDispatchOnConnect, args));
}

static void DispatchOnDisconnect(
    const ExtensionMessageService::MessagePort& port, int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  port.sender->Send(new ViewMsg_ExtensionMessageInvoke(
      port.routing_id, ExtensionMessageService::kDispatchOnDisconnect, args));
}

static void DispatchOnMessage(const ExtensionMessageService::MessagePort& port,
                              const std::string& message, int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(message));
  args.Set(1, Value::CreateIntegerValue(source_port_id));
  port.sender->Send(new ViewMsg_ExtensionMessageInvoke(
      port.routing_id, ExtensionMessageService::kDispatchOnMessage, args));
}

static void DispatchEvent(const ExtensionMessageService::MessagePort& port,
                          const std::string& event_name,
                          const std::string& event_args) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  port.sender->Send(new ViewMsg_ExtensionMessageInvoke(
      port.routing_id, ExtensionMessageService::kDispatchEvent, args));
}

}  // namespace

const char ExtensionMessageService::kDispatchOnConnect[] =
    "Port.dispatchOnConnect";
const char ExtensionMessageService::kDispatchOnDisconnect[] =
    "Port.dispatchOnDisconnect";
const char ExtensionMessageService::kDispatchOnMessage[] =
    "Port.dispatchOnMessage";
const char ExtensionMessageService::kDispatchEvent[] =
    "Event.dispatchJSON";

ExtensionMessageService::ExtensionMessageService(Profile* profile)
    : ui_loop_(MessageLoop::current()), profile_(profile), next_port_id_(0) {
  DCHECK_EQ(ui_loop_->type(), MessageLoop::TYPE_UI);

  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_DELETED,
                 NotificationService::AllSources());
}

ExtensionMessageService::~ExtensionMessageService() {
}

void ExtensionMessageService::ProfileDestroyed() {
  DCHECK_EQ(ui_loop_->type(), MessageLoop::TYPE_UI);

  profile_ = NULL;

  // We remove notifications here because our destructor might be called on
  // a non-UI thread.
  registrar_.RemoveAll();
}

void ExtensionMessageService::AddEventListener(std::string event_name,
                                               int render_process_id) {
  DCHECK(RenderProcessHost::FromID(render_process_id)) <<
      "Adding event listener to a non-existant RenderProcessHost.";
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 0u) << event_name;
  listeners_[event_name].insert(render_process_id);
}

void ExtensionMessageService::RemoveEventListener(std::string event_name,
                                                  int render_process_id) {
  // It is possible that this RenderProcessHost is being destroyed.  If that is
  // the case, we'll have already removed his listeners, so do nothing here.
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 1u) << event_name;
  listeners_[event_name].erase(render_process_id);
}

void ExtensionMessageService::AllocatePortIdPair(int* port1, int* port2) {
  AutoLock lock(next_port_id_lock_);

  // TODO(mpcomplete): what happens when this wraps?
  int port1_id = next_port_id_++;
  int port2_id = next_port_id_++;

  DCHECK(IS_OPENER_PORT_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));

  int channel_id = GET_CHANNEL_ID(port1_id);
  DCHECK(GET_CHANNEL_OPENER_ID(channel_id) == port1_id);
  DCHECK(GET_CHANNEL_RECEIVERS_ID(channel_id) == port2_id);

  *port1 = port1_id;
  *port2 = port2_id;
}

int ExtensionMessageService::OpenChannelToExtension(
    int routing_id, const std::string& extension_id,
    const std::string& channel_name, ResourceMessageFilter* source) {
  DCHECK_EQ(MessageLoop::current(),
            ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  // Each side of the port is given his own port ID.  When they send messages,
  // we convert to the opposite port ID.  See PostMessageFromRenderer.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
          &ExtensionMessageService::OpenChannelToExtensionOnUIThread,
          source->GetProcessId(), routing_id, port2_id, extension_id,
          channel_name));

  return port1_id;
}

int ExtensionMessageService::OpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, ResourceMessageFilter* source) {
  DCHECK_EQ(MessageLoop::current(),
            ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  // Each side of the port is given his own port ID.  When they send messages,
  // we convert to the opposite port ID.  See PostMessageFromRenderer.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
          &ExtensionMessageService::OpenChannelToTabOnUIThread,
          source->GetProcessId(), routing_id, port2_id, tab_id, extension_id,
          channel_name));

  return port1_id;
}

void ExtensionMessageService::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& extension_id, const std::string& channel_name) {
  if (!profile_)
    return;

  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  MessagePort receiver(
      profile_->GetExtensionProcessManager()->GetExtensionProcess(extension_id),
      MSG_ROUTING_CONTROL);
  OpenChannelOnUIThreadImpl(source, source_process_id, source_routing_id,
                            receiver, receiver_port_id, extension_id,
                            channel_name);
}

void ExtensionMessageService::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    int tab_id, const std::string& extension_id,
    const std::string& channel_name) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  TabContents* contents;
  MessagePort receiver;
  if (ExtensionTabUtil::GetTabById(tab_id, source->profile(),
                                   NULL, NULL, &contents, NULL)) {
    receiver.sender = contents->render_view_host();
    receiver.routing_id = contents->render_view_host()->routing_id();
  }
  OpenChannelOnUIThreadImpl(source, source_process_id, source_routing_id,
                            receiver, receiver_port_id, extension_id,
                            channel_name);
}

void ExtensionMessageService::OpenChannelOnUIThreadImpl(
    IPC::Message::Sender* source, int source_process_id, int source_routing_id,
    const MessagePort& receiver, int receiver_port_id,
    const std::string& extension_id, const std::string& channel_name) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // TODO(mpcomplete): notify source if reciever doesn't exist
  if (!source)
    return;  // Closed while in flight.

  if (!receiver.sender) {
    // Treat it as a disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL),
                         GET_OPPOSITE_PORT_ID(receiver_port_id));
  }

  linked_ptr<MessageChannel> channel(new MessageChannel);
  channel->opener = MessagePort(source, MSG_ROUTING_CONTROL);
  channel->receiver = receiver;

  channels_[GET_CHANNEL_ID(receiver_port_id)] = channel;

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  TabContents* contents = tab_util::GetTabContentsByID(source_process_id,
                                                       source_routing_id);
  if (contents) {
    DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(contents);
    JSONWriter::Write(tab_value, false, &tab_json);
  }

  // Send the connect event to the receiver.  Give it the opener's port ID (the
  // opener has the opposite port ID).
  DispatchOnConnect(receiver, receiver_port_id, channel_name, tab_json,
                    extension_id);
}

int ExtensionMessageService::OpenAutomationChannelToExtension(
    int source_process_id, int routing_id, const std::string& extension_id,
    IPC::Message::Sender* source) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(profile_);

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  // TODO(siggi): The source process- and routing ids are used to
  //      describe the originating tab to the target extension.
  //      This isn't really appropriate here, the originating tab
  //      information should be supplied by the caller for
  //      automation-initiated ports.
  MessagePort receiver(
      profile_->GetExtensionProcessManager()->GetExtensionProcess(extension_id),
      MSG_ROUTING_CONTROL);
  OpenChannelOnUIThreadImpl(source, source_process_id, routing_id, receiver,
                            port2_id, extension_id, "");

  return port1_id;
}

void ExtensionMessageService::CloseChannel(int port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Note: The channel might be gone already, if the other side closed first.
  MessageChannelMap::iterator it = channels_.find(GET_CHANNEL_ID(port_id));
  if (it != channels_.end())
    CloseChannelImpl(it, port_id);
}

void ExtensionMessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter, int closing_port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Notify the other side.
  const MessagePort& port = IS_OPENER_PORT_ID(closing_port_id) ?
      channel_iter->second->receiver : channel_iter->second->opener;

  DispatchOnDisconnect(port, GET_OPPOSITE_PORT_ID(closing_port_id));
  channels_.erase(channel_iter);
}

void ExtensionMessageService::PostMessageFromRenderer(
    int source_port_id, const std::string& message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannelMap::iterator iter =
      channels_.find(GET_CHANNEL_ID(source_port_id));
  if (iter == channels_.end())
    return;

  // Figure out which port the ID corresponds to.
  int dest_port_id = GET_OPPOSITE_PORT_ID(source_port_id);
  const MessagePort& port = IS_OPENER_PORT_ID(dest_port_id) ?
      iter->second->opener : iter->second->receiver;

  DispatchOnMessage(port, message, dest_port_id);
}

void ExtensionMessageService::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  std::set<int>& pids = listeners_[event_name];

  // Send the event only to renderers that are listening for it.
  for (std::set<int>::iterator pid = pids.begin(); pid != pids.end(); ++pid) {
    RenderProcessHost* renderer = RenderProcessHost::FromID(*pid);
    if (!renderer)
      continue;
    if (!ChildProcessSecurityPolicy::GetInstance()->
            HasExtensionBindings(*pid)) {
      // Don't send browser-level events to unprivileged processes.
      continue;
    }

    DispatchEvent(renderer, event_name, event_args);
  }
}

void ExtensionMessageService::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  switch (type.value) {
    case NotificationType::RENDERER_PROCESS_TERMINATED:
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* renderer = Source<RenderProcessHost>(source).ptr();
      OnSenderClosed(renderer);

      // Remove this renderer from our listener maps.
      for (ListenerMap::iterator it = listeners_.begin();
           it != listeners_.end(); ) {
        ListenerMap::iterator current = it++;
        current->second.erase(renderer->pid());
        if (current->second.empty())
          listeners_.erase(current);
      }
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_DELETED:
      OnSenderClosed(Details<RenderViewHost>(details).ptr());
      break;
    default:
      NOTREACHED();
      return;
  }
}

void ExtensionMessageService::OnSenderClosed(IPC::Message::Sender* sender) {
  // Close any channels that share this renderer.  We notify the opposite
  // port that his pair has closed.
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second->opener.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first));
    } else if (current->second->receiver.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first));
    }
  }
}
