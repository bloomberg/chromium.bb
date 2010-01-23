// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/json/json_writer.h"
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
  // TODO(mpcomplete): remove this when I track down the crasher. Hopefully
  // this guy will show up in some stack traces and potentially give some
  // insight.
  // http://code.google.com/p/chromium/issues/detail?id=21201
  int debug_info;

  MessagePort(IPC::Message::Sender* sender = NULL,
              int routing_id = MSG_ROUTING_CONTROL) :
     sender(sender), routing_id(routing_id), debug_info(0) {}
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
                              const std::string& source_extension_id,
                              const std::string& target_extension_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(dest_port_id));
  args.Set(1, Value::CreateStringValue(channel_name));
  args.Set(2, Value::CreateStringValue(tab_json));
  args.Set(3, Value::CreateStringValue(source_extension_id));
  args.Set(4, Value::CreateStringValue(target_extension_id));
  CHECK(port.sender);
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
    : profile_(profile),
      extension_devtools_manager_(NULL),
      next_port_id_(0) {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_DELETED,
                 NotificationService::AllSources());

  extension_devtools_manager_ = profile_->GetExtensionDevToolsManager();
}

ExtensionMessageService::~ExtensionMessageService() {
  STLDeleteContainerPairSecondPointers(channels_.begin(), channels_.end());
  channels_.clear();
}

void ExtensionMessageService::ProfileDestroyed() {
  profile_ = NULL;
  if (!registrar_.IsEmpty()) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    registrar_.RemoveAll();
  }
}

void ExtensionMessageService::AddEventListener(const std::string& event_name,
                                               int render_process_id) {
  DCHECK(RenderProcessHost::FromID(render_process_id)) <<
      "Adding event listener to a non-existant RenderProcessHost.";
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 0u) << event_name;
  listeners_[event_name].insert(render_process_id);

  if (extension_devtools_manager_.get()) {
    extension_devtools_manager_->AddEventListener(event_name,
                                                  render_process_id);
  }
}

void ExtensionMessageService::RemoveEventListener(const std::string& event_name,
                                                  int render_process_id) {
  // It is possible that this RenderProcessHost is being destroyed.  If that is
  // the case, we'll have already removed his listeners, so do nothing here.
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id);
  if (!rph || rph->ListenersIterator().IsAtEnd())
    return;

  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 1u)
      << " PID=" << render_process_id << " event=" << event_name;
  listeners_[event_name].erase(render_process_id);

  if (extension_devtools_manager_.get()) {
    extension_devtools_manager_->RemoveEventListener(event_name,
                                                     render_process_id);
  }
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
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, ResourceMessageFilter* source) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  // Each side of the port is given his own port ID.  When they send messages,
  // we convert to the opposite port ID.  See PostMessageFromRenderer.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionMessageService::OpenChannelToExtensionOnUIThread,
          source->id(), routing_id, port2_id, source_extension_id,
          target_extension_id, channel_name));

  return port1_id;
}

int ExtensionMessageService::OpenChannelToTab(int routing_id,
                                              int tab_id,
                                              const std::string& extension_id,
                                              const std::string& channel_name,
                                              ResourceMessageFilter* source) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  // Each side of the port is given his own port ID.  When they send messages,
  // we convert to the opposite port ID.  See PostMessageFromRenderer.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionMessageService::OpenChannelToTabOnUIThread,
          source->id(), routing_id, port2_id, tab_id, extension_id,
          channel_name));

  return port1_id;
}

void ExtensionMessageService::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  if (!profile_)
    return;

  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

  MessagePort receiver(
      profile_->GetExtensionProcessManager()->GetExtensionProcess(
          target_extension_id),
      MSG_ROUTING_CONTROL);
  receiver.debug_info = 1;
  TabContents* source_contents = tab_util::GetTabContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  if (source_contents) {
    scoped_ptr<DictionaryValue> tab_value(
        ExtensionTabUtil::CreateTabValue(source_contents));
    base::JSONWriter::Write(tab_value.get(), false, &tab_json);
  }

  OpenChannelOnUIThreadImpl(source, tab_json,
                            receiver, receiver_port_id,
                            source_extension_id, target_extension_id,
                            channel_name);
}

void ExtensionMessageService::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    int tab_id, const std::string& extension_id,
    const std::string& channel_name) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

  TabContents* contents = NULL;
  MessagePort receiver;
  receiver.debug_info = 2;
  if (ExtensionTabUtil::GetTabById(tab_id, source->profile(),
                                   NULL, NULL, &contents, NULL)) {
    receiver.sender = contents->render_view_host();
    receiver.routing_id = contents->render_view_host()->routing_id();
    receiver.debug_info = 3;
  }

  if (contents && contents->controller().needs_reload()) {
    // The tab isn't loaded yet (it may be phantom). Don't attempt to connect.
    // Treat this as a disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL),
                         GET_OPPOSITE_PORT_ID(receiver_port_id));
    return;
  }

  TabContents* source_contents = tab_util::GetTabContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  if (source_contents) {
    scoped_ptr<DictionaryValue> tab_value(
        ExtensionTabUtil::CreateTabValue(source_contents));
    base::JSONWriter::Write(tab_value.get(), false, &tab_json);
  }

  OpenChannelOnUIThreadImpl(source, tab_json,
                            receiver, receiver_port_id,
                            extension_id, extension_id, channel_name);
}

bool ExtensionMessageService::OpenChannelOnUIThreadImpl(
    IPC::Message::Sender* source,
    const std::string& tab_json,
    const MessagePort& receiver, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // TODO(mpcomplete): notify source if reciever doesn't exist
  if (!source)
    return false;  // Closed while in flight.

  if (!receiver.sender) {
    // Treat it as a disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL),
                         GET_OPPOSITE_PORT_ID(receiver_port_id));
    return false;
  }

  // Add extra paranoid CHECKs, since we have crash reports of this being NULL.
  // http://code.google.com/p/chromium/issues/detail?id=19067
  CHECK(receiver.sender);

  MessageChannel* channel(new MessageChannel);
  channel->opener = MessagePort(source, MSG_ROUTING_CONTROL);
  channel->receiver = receiver;

  CHECK(receiver.sender);

  CHECK(channels_.find(GET_CHANNEL_ID(receiver_port_id)) == channels_.end());
  channels_[GET_CHANNEL_ID(receiver_port_id)] = channel;

  CHECK(receiver.sender);

  // Send the connect event to the receiver.  Give it the opener's port ID (the
  // opener has the opposite port ID).
  DispatchOnConnect(receiver, receiver_port_id, channel_name, tab_json,
                    source_extension_id, target_extension_id);

  return true;
}

int ExtensionMessageService::OpenSpecialChannelToExtension(
    const std::string& extension_id, const std::string& channel_name,
    const std::string& tab_json, IPC::Message::Sender* source) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(profile_);

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  MessagePort receiver(
      profile_->GetExtensionProcessManager()->
          GetExtensionProcess(extension_id),
      MSG_ROUTING_CONTROL);
  receiver.debug_info = 4;
  if (!OpenChannelOnUIThreadImpl(
      source, tab_json, receiver, port2_id, extension_id, extension_id,
      channel_name))
    return -1;

  return port1_id;
}

int ExtensionMessageService::OpenSpecialChannelToTab(
    const std::string& extension_id, const std::string& channel_name,
    TabContents* target_tab_contents, IPC::Message::Sender* source) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(target_tab_contents);

  if (target_tab_contents->controller().needs_reload()) {
    // The tab isn't loaded yet (it may be phantom). Don't attempt to connect.
    return -1;
  }

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  MessagePort receiver(
      target_tab_contents->render_view_host(),
      target_tab_contents->render_view_host()->routing_id());
  receiver.debug_info = 5;
  if (!OpenChannelOnUIThreadImpl(source, "null",
                                 receiver, port2_id,
                                 extension_id, extension_id, channel_name))
    return -1;

  return port1_id;
}

void ExtensionMessageService::CloseChannel(int port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Note: The channel might be gone already, if the other side closed first.
  MessageChannelMap::iterator it = channels_.find(GET_CHANNEL_ID(port_id));
  if (it != channels_.end())
    CloseChannelImpl(it, port_id, true);
}

void ExtensionMessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter, int closing_port_id,
    bool notify_other_port) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Notify the other side.
  const MessagePort& port = IS_OPENER_PORT_ID(closing_port_id) ?
      channel_iter->second->receiver : channel_iter->second->opener;

  if (notify_other_port)
    DispatchOnDisconnect(port, GET_OPPOSITE_PORT_ID(closing_port_id));
  delete channel_iter->second;
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

      // Remove all event listeners associated with this renderer
      for (ListenerMap::iterator it = listeners_.begin();
           it != listeners_.end(); ) {
        ListenerMap::iterator current = it++;
        if (current->second.count(renderer->id()) != 0)
          RemoveEventListener(current->first, renderer->id());
      }
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_DELETED:
      OnSenderClosed(Details<RenderViewHost>(details).ptr());
      break;

    // We should already have removed this guy from our channel map by this
    // point.
    case NotificationType::EXTENSION_PORT_DELETED_DEBUG: {
      IPC::Message::Sender* sender =
          Details<IPC::Message::Sender>(details).ptr();
      for (MessageChannelMap::iterator it = channels_.begin();
           it != channels_.end(); ) {
        MessageChannelMap::iterator current = it++;
        int debug_info = current->second->receiver.debug_info;
        if (current->second->opener.sender == sender) {
          CHECK(false) << "Shouldn't happen:" << debug_info;
        } else if (current->second->receiver.sender == sender) {
          CHECK(false) << "Shouldn't happen either: " << debug_info;
        }
      }
      OnSenderClosed(sender);
      break;
    }

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
    // If both sides are the same renderer, and it is closing, there is no
    // "other" port, so there's no need to notify it.
    int debug_info = current->second->receiver.debug_info;
    bool debug_check = debug_info == 4 || debug_info == 5;
    bool notify_other_port =
        current->second->opener.sender != current->second->receiver.sender ||
        debug_check;

    if (current->second->opener.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first),
                       notify_other_port);
    } else if (current->second->receiver.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first),
                       notify_other_port);
    }
  }
}
