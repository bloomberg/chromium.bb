// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/atomic_sequence_num.h"
#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

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
  explicit MessagePort(IPC::Message::Sender* sender = NULL,
              int routing_id = MSG_ROUTING_CONTROL)
     : sender(sender), routing_id(routing_id) {}
};

struct ExtensionMessageService::MessageChannel {
  ExtensionMessageService::MessagePort opener;
  ExtensionMessageService::MessagePort receiver;
};

const char ExtensionMessageService::kDispatchOnConnect[] =
    "Port.dispatchOnConnect";
const char ExtensionMessageService::kDispatchOnDisconnect[] =
    "Port.dispatchOnDisconnect";
const char ExtensionMessageService::kDispatchOnMessage[] =
    "Port.dispatchOnMessage";

namespace {

static base::AtomicSequenceNumber g_next_channel_id(base::LINKER_INITIALIZED);

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
  port.sender->Send(new ExtensionMsg_MessageInvoke(port.routing_id,
       "", ExtensionMessageService::kDispatchOnConnect, args, GURL()));
}

static void DispatchOnDisconnect(
    const ExtensionMessageService::MessagePort& port, int source_port_id,
    bool connection_error) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  args.Set(1, Value::CreateBooleanValue(connection_error));
  port.sender->Send(new ExtensionMsg_MessageInvoke(port.routing_id,
      "", ExtensionMessageService::kDispatchOnDisconnect, args, GURL()));
}

static void DispatchOnMessage(const ExtensionMessageService::MessagePort& port,
                              const std::string& message, int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(message));
  args.Set(1, Value::CreateIntegerValue(source_port_id));
  port.sender->Send(new ExtensionMsg_MessageInvoke(port.routing_id,
      "", ExtensionMessageService::kDispatchOnMessage, args, GURL()));
}

}  // namespace

// static
void ExtensionMessageService::AllocatePortIdPair(int* port1, int* port2) {
  int channel_id = g_next_channel_id.GetNext();
  int port1_id = channel_id * 2;
  int port2_id = channel_id * 2 + 1;

  // Sanity checks to make sure our channel<->port converters are correct.
  DCHECK(IS_OPENER_PORT_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));
  DCHECK(GET_CHANNEL_ID(port1_id) == channel_id);
  DCHECK(GET_CHANNEL_OPENER_ID(channel_id) == port1_id);
  DCHECK(GET_CHANNEL_RECEIVERS_ID(channel_id) == port2_id);

  *port1 = port1_id;
  *port2 = port2_id;
}

ExtensionMessageService::ExtensionMessageService(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_DELETED,
                 NotificationService::AllSources());
}

ExtensionMessageService::~ExtensionMessageService() {
  STLDeleteContainerPairSecondPointers(channels_.begin(), channels_.end());
  channels_.clear();
}

void ExtensionMessageService::DestroyingProfile() {
  profile_ = NULL;
  if (!registrar_.IsEmpty())
    registrar_.RemoveAll();
}

void ExtensionMessageService::OpenChannelToExtension(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

  // Note: we use the source's profile here. If the source is an incognito
  // process, we will use the incognito EPM to find the right extension process,
  // which depends on whether the extension uses spanning or split mode.
  MessagePort receiver(
      source->profile()->GetExtensionProcessManager()->GetExtensionProcess(
          target_extension_id),
      MSG_ROUTING_CONTROL);
  TabContents* source_contents = tab_util::GetTabContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  if (source_contents) {
    scoped_ptr<DictionaryValue> tab_value(
        ExtensionTabUtil::CreateTabValue(source_contents));
    base::JSONWriter::Write(tab_value.get(), false, &tab_json);
  }

  OpenChannelImpl(source, tab_json, receiver, receiver_port_id,
                  source_extension_id, target_extension_id, channel_name);
}

void ExtensionMessageService::OpenChannelToTab(
    int source_process_id, int source_routing_id, int receiver_port_id,
    int tab_id, const std::string& extension_id,
    const std::string& channel_name) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

  TabContentsWrapper* contents = NULL;
  MessagePort receiver;
  if (ExtensionTabUtil::GetTabById(tab_id, source->profile(), true,
                                   NULL, NULL, &contents, NULL)) {
    receiver.sender = contents->render_view_host();
    receiver.routing_id = contents->render_view_host()->routing_id();
  }

  if (contents && contents->controller().needs_reload()) {
    // The tab isn't loaded yet. Don't attempt to connect. Treat this as a
    // disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL),
                         GET_OPPOSITE_PORT_ID(receiver_port_id), true);
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

  OpenChannelImpl(source, tab_json, receiver, receiver_port_id,
                  extension_id, extension_id, channel_name);
}

bool ExtensionMessageService::OpenChannelImpl(
    IPC::Message::Sender* source,
    const std::string& tab_json,
    const MessagePort& receiver, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  if (!source)
    return false;  // Closed while in flight.

  if (!receiver.sender) {
    // Treat it as a disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL),
                         GET_OPPOSITE_PORT_ID(receiver_port_id), true);
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
  DCHECK(profile_);

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  MessagePort receiver(
      profile_->GetExtensionProcessManager()->GetExtensionProcess(extension_id),
      MSG_ROUTING_CONTROL);
  if (!OpenChannelImpl(source, tab_json, receiver, port2_id,
                       extension_id, extension_id, channel_name))
    return -1;

  return port1_id;
}

int ExtensionMessageService::OpenSpecialChannelToTab(
    const std::string& extension_id, const std::string& channel_name,
    TabContents* target_tab_contents, IPC::Message::Sender* source) {
  DCHECK(target_tab_contents);

  if (target_tab_contents->controller().needs_reload()) {
    // The tab isn't loaded yet. Don't attempt to connect.
    return -1;
  }

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  MessagePort receiver(
      target_tab_contents->render_view_host(),
      target_tab_contents->render_view_host()->routing_id());
  if (!OpenChannelImpl(source, "null", receiver, port2_id,
                       extension_id, extension_id, channel_name))
    return -1;

  return port1_id;
}

void ExtensionMessageService::CloseChannel(int port_id) {
  // Note: The channel might be gone already, if the other side closed first.
  MessageChannelMap::iterator it = channels_.find(GET_CHANNEL_ID(port_id));
  if (it != channels_.end())
    CloseChannelImpl(it, port_id, true);
}

void ExtensionMessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter, int closing_port_id,
    bool notify_other_port) {
  // Notify the other side.
  const MessagePort& port = IS_OPENER_PORT_ID(closing_port_id) ?
      channel_iter->second->receiver : channel_iter->second->opener;

  if (notify_other_port)
    DispatchOnDisconnect(port, GET_OPPOSITE_PORT_ID(closing_port_id), false);
  delete channel_iter->second;
  channels_.erase(channel_iter);
}

void ExtensionMessageService::PostMessageFromRenderer(
    int source_port_id, const std::string& message) {
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
void ExtensionMessageService::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::RENDERER_PROCESS_TERMINATED:
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* renderer = Source<RenderProcessHost>(source).ptr();
      OnSenderClosed(renderer);
      break;
    }
    case NotificationType::RENDER_VIEW_HOST_DELETED:
      OnSenderClosed(Source<RenderViewHost>(source).ptr());
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
    // If both sides are the same renderer, and it is closing, there is no
    // "other" port, so there's no need to notify it.
    bool notify_other_port =
        current->second->opener.sender != current->second->receiver.sender;

    if (current->second->opener.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first),
                       notify_other_port);
    } else if (current->second->receiver.sender == sender) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first),
                       notify_other_port);
    }
  }
}
