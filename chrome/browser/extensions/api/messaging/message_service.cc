// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/message_service.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/view_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

using content::SiteInstance;
using content::WebContents;

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) ((port_id) / 2)
#define GET_CHANNEL_OPENER_ID(channel_id) ((channel_id) * 2)
#define GET_CHANNEL_RECEIVERS_ID(channel_id) ((channel_id) * 2 + 1)

// Port1 is always even, port2 is always odd.
#define IS_OPENER_PORT_ID(port_id) (((port_id) & 1) == 0)

// Change even to odd and vice versa, to get the other side of a given channel.
#define GET_OPPOSITE_PORT_ID(source_port_id) ((source_port_id) ^ 1)

namespace extensions {

struct MessageService::MessagePort {
  content::RenderProcessHost* process;
  int routing_id;
  std::string extension_id;
  void* background_host_ptr;  // used in IncrementLazyKeepaliveCount

  MessagePort()
     : process(NULL),
       routing_id(MSG_ROUTING_CONTROL),
       background_host_ptr(NULL) {}
  MessagePort(content::RenderProcessHost* process,
              int routing_id,
              const std::string& extension_id)
     : process(process),
       routing_id(routing_id),
       extension_id(extension_id),
       background_host_ptr(NULL) {}
};

struct MessageService::MessageChannel {
  MessageService::MessagePort opener;
  MessageService::MessagePort receiver;
};

struct MessageService::OpenChannelParams {
  content::RenderProcessHost* source;
  std::string tab_json;
  MessagePort receiver;
  int receiver_port_id;
  std::string source_extension_id;
  std::string target_extension_id;
  std::string channel_name;

  OpenChannelParams(content::RenderProcessHost* source,
                    const std::string& tab_json,
                    const MessagePort& receiver,
                    int receiver_port_id,
                    const std::string& source_extension_id,
                    const std::string& target_extension_id,
                    const std::string& channel_name)
      : source(source),
        tab_json(tab_json),
        receiver(receiver),
        receiver_port_id(receiver_port_id),
        source_extension_id(source_extension_id),
        target_extension_id(target_extension_id),
        channel_name(channel_name) {}
};

namespace {

static base::StaticAtomicSequenceNumber g_next_channel_id;

static void DispatchOnConnect(const MessageService::MessagePort& port,
                              int dest_port_id,
                              const std::string& channel_name,
                              const std::string& tab_json,
                              const std::string& source_extension_id,
                              const std::string& target_extension_id) {
  port.process->Send(new ExtensionMsg_DispatchOnConnect(
      port.routing_id, dest_port_id, channel_name,
      tab_json, source_extension_id, target_extension_id));
}

static void DispatchOnDisconnect(const MessageService::MessagePort& port,
                                 int source_port_id,
                                 bool connection_error) {
  port.process->Send(new ExtensionMsg_DispatchOnDisconnect(
      port.routing_id, source_port_id, connection_error));
}

static void DispatchOnMessage(const MessageService::MessagePort& port,
                              const std::string& message,
                              int target_port_id) {
  port.process->Send(new ExtensionMsg_DeliverMessage(
      port.routing_id, target_port_id, message));
}

static content::RenderProcessHost* GetExtensionProcess(
    Profile* profile, const std::string& extension_id) {
  SiteInstance* site_instance =
      profile->GetExtensionProcessManager()->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(extension_id));

  if (!site_instance->HasProcess())
    return NULL;

  return site_instance->GetProcess();
}

static void IncrementLazyKeepaliveCount(MessageService::MessagePort* port) {
  Profile* profile =
      Profile::FromBrowserContext(port->process->GetBrowserContext());
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(port->extension_id);
  if (host && host->extension()->has_lazy_background_page())
    pm->IncrementLazyKeepaliveCount(host->extension());

  // Keep track of the background host, so when we decrement, we only do so if
  // the host hasn't reloaded.
  port->background_host_ptr = host;
}

static void DecrementLazyKeepaliveCount(MessageService::MessagePort* port) {
  Profile* profile =
      Profile::FromBrowserContext(port->process->GetBrowserContext());
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(port->extension_id);
  if (host && host == port->background_host_ptr)
    pm->DecrementLazyKeepaliveCount(host->extension());
}

}  // namespace

// static
void MessageService::AllocatePortIdPair(int* port1, int* port2) {
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

MessageService::MessageService(
    LazyBackgroundTaskQueue* queue)
    : lazy_background_task_queue_(queue) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

MessageService::~MessageService() {
  STLDeleteContainerPairSecondPointers(channels_.begin(), channels_.end());
  channels_.clear();
}

void MessageService::OpenChannelToExtension(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

  // Note: we use the source's profile here. If the source is an incognito
  // process, we will use the incognito EPM to find the right extension process,
  // which depends on whether the extension uses spanning or split mode.
  MessagePort receiver(GetExtensionProcess(profile, target_extension_id),
                       MSG_ROUTING_CONTROL,
                       target_extension_id);
  WebContents* source_contents = tab_util::GetWebContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  if (source_contents) {
    scoped_ptr<DictionaryValue> tab_value(ExtensionTabUtil::CreateTabValue(
            source_contents,
            profile->GetExtensionService()->extensions()->GetByID(
                source_extension_id)));
    base::JSONWriter::Write(tab_value.get(), &tab_json);
  }

  OpenChannelParams params(source, tab_json, receiver, receiver_port_id,
                           source_extension_id, target_extension_id,
                           channel_name);

  // The target might be a lazy background page. In that case, we have to check
  // if it is loaded and ready, and if not, queue up the task and load the
  // page.
  if (MaybeAddPendingOpenChannelTask(profile, params))
    return;

  OpenChannelImpl(params);
}

void MessageService::OpenChannelToTab(
    int source_process_id, int source_routing_id, int receiver_port_id,
    int tab_id, const std::string& extension_id,
    const std::string& channel_name) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

  TabContents* contents = NULL;
  MessagePort receiver;
  if (ExtensionTabUtil::GetTabById(tab_id, profile, true,
                                   NULL, NULL, &contents, NULL)) {
    receiver.process = contents->web_contents()->GetRenderProcessHost();
    receiver.routing_id =
        contents->web_contents()->GetRenderViewHost()->GetRoutingID();
    receiver.extension_id = extension_id;
  }

  if (contents && contents->web_contents()->GetController().NeedsReload()) {
    // The tab isn't loaded yet. Don't attempt to connect. Treat this as a
    // disconnect.
    DispatchOnDisconnect(MessagePort(source, MSG_ROUTING_CONTROL, extension_id),
                         GET_OPPOSITE_PORT_ID(receiver_port_id), true);
    return;
  }

  WebContents* source_contents = tab_util::GetWebContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  if (source_contents) {
    scoped_ptr<DictionaryValue> tab_value(ExtensionTabUtil::CreateTabValue(
            source_contents,
            profile->GetExtensionService()->extensions()->GetByID(
                extension_id)));
    base::JSONWriter::Write(tab_value.get(), &tab_json);
  }

  OpenChannelParams params(source, tab_json, receiver, receiver_port_id,
                           extension_id, extension_id, channel_name);
  OpenChannelImpl(params);
}

bool MessageService::OpenChannelImpl(const OpenChannelParams& params) {
  if (!params.source)
    return false;  // Closed while in flight.

  if (!params.receiver.process) {
    // Treat it as a disconnect.
    DispatchOnDisconnect(MessagePort(params.source, MSG_ROUTING_CONTROL, ""),
                         GET_OPPOSITE_PORT_ID(params.receiver_port_id), true);
    return false;
  }

  // Add extra paranoid CHECKs, since we have crash reports of this being NULL.
  // http://code.google.com/p/chromium/issues/detail?id=19067
  CHECK(params.receiver.process);

  MessageChannel* channel(new MessageChannel);
  channel->opener = MessagePort(params.source, MSG_ROUTING_CONTROL,
                                params.source_extension_id);
  channel->receiver = params.receiver;

  CHECK(params.receiver.process);

  int channel_id = GET_CHANNEL_ID(params.receiver_port_id);
  CHECK(channels_.find(channel_id) == channels_.end());
  channels_[channel_id] = channel;
  pending_channels_.erase(channel_id);

  CHECK(params.receiver.process);

  // Send the connect event to the receiver.  Give it the opener's port ID (the
  // opener has the opposite port ID).
  DispatchOnConnect(params.receiver, params.receiver_port_id,
                    params.channel_name, params.tab_json,
                    params.source_extension_id, params.target_extension_id);

  // Keep both ends of the channel alive until the channel is closed.
  IncrementLazyKeepaliveCount(&channel->opener);
  IncrementLazyKeepaliveCount(&channel->receiver);
  return true;
}

void MessageService::CloseChannel(int port_id, bool connection_error) {
  // Note: The channel might be gone already, if the other side closed first.
  int channel_id = GET_CHANNEL_ID(port_id);
  MessageChannelMap::iterator it = channels_.find(channel_id);
  if (it == channels_.end()) {
    PendingChannelMap::iterator pending = pending_channels_.find(channel_id);
    if (pending != pending_channels_.end()) {
      lazy_background_task_queue_->AddPendingTask(
          pending->second.first, pending->second.second,
          base::Bind(&MessageService::PendingCloseChannel,
                     base::Unretained(this), port_id, connection_error));
    }
    return;
  }
  CloseChannelImpl(it, port_id, connection_error, true);
}

void MessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter, int closing_port_id,
    bool connection_error, bool notify_other_port) {
  MessageChannel* channel = channel_iter->second;

  // Notify the other side.
  if (notify_other_port) {
    const MessagePort& port = IS_OPENER_PORT_ID(closing_port_id) ?
        channel->receiver : channel->opener;
    DispatchOnDisconnect(port, GET_OPPOSITE_PORT_ID(closing_port_id),
                         connection_error);
  }

  // Balance the addrefs in OpenChannelImpl.
  DecrementLazyKeepaliveCount(&channel->opener);
  DecrementLazyKeepaliveCount(&channel->receiver);

  delete channel_iter->second;
  channels_.erase(channel_iter);
}

void MessageService::PostMessageFromRenderer(
    int source_port_id, const std::string& message) {
  int channel_id = GET_CHANNEL_ID(source_port_id);
  MessageChannelMap::iterator iter = channels_.find(channel_id);
  if (iter == channels_.end()) {
    // If this channel is pending, queue up the PostMessage to run once
    // the channel opens.
    PendingChannelMap::iterator pending = pending_channels_.find(channel_id);
    if (pending != pending_channels_.end()) {
      lazy_background_task_queue_->AddPendingTask(
          pending->second.first, pending->second.second,
          base::Bind(&MessageService::PendingPostMessage,
                     base::Unretained(this), source_port_id, message));
    }
    return;
  }

  // Figure out which port the ID corresponds to.
  int dest_port_id = GET_OPPOSITE_PORT_ID(source_port_id);
  const MessagePort& port = IS_OPENER_PORT_ID(dest_port_id) ?
      iter->second->opener : iter->second->receiver;

  DispatchOnMessage(port, message, dest_port_id);
}

void MessageService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost* renderer =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnProcessClosed(renderer);
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

void MessageService::OnProcessClosed(content::RenderProcessHost* process) {
  // Close any channels that share this renderer.  We notify the opposite
  // port that his pair has closed.
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    // If both sides are the same renderer, and it is closing, there is no
    // "other" port, so there's no need to notify it.
    bool notify_other_port =
        current->second->opener.process != current->second->receiver.process;

    if (current->second->opener.process == process) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first),
                       false, notify_other_port);
    } else if (current->second->receiver.process == process) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first),
                       false, notify_other_port);
    }
  }
}

bool MessageService::MaybeAddPendingOpenChannelTask(
    Profile* profile,
    const OpenChannelParams& params) {
  ExtensionService* service = profile->GetExtensionService();
  const std::string& extension_id = params.target_extension_id;
  const Extension* extension = service->extensions()->GetByID(
      extension_id);
  if (extension && extension->has_lazy_background_page()) {
    // If the extension uses spanning incognito mode, make sure we're always
    // using the original profile since that is what the extension process
    // will use.
    if (!extension->incognito_split_mode())
      profile = profile->GetOriginalProfile();

    if (lazy_background_task_queue_->ShouldEnqueueTask(profile, extension)) {
      lazy_background_task_queue_->AddPendingTask(profile, extension_id,
          base::Bind(&MessageService::PendingOpenChannel,
                     base::Unretained(this), params, params.source->GetID()));
      pending_channels_[GET_CHANNEL_ID(params.receiver_port_id)] =
          PendingChannel(profile, extension_id);
      return true;
    }
  }

  return false;
}

void MessageService::PendingOpenChannel(const OpenChannelParams& params_in,
                                        int source_process_id,
                                        ExtensionHost* host) {
  if (!host)
    return;  // TODO(mpcomplete): notify source of disconnect?

  // Re-lookup the source process since it may no longer be valid.
  OpenChannelParams params = params_in;
  params.source = content::RenderProcessHost::FromID(source_process_id);
  if (!params.source)
    return;

  params.receiver = MessagePort(host->render_process_host(),
                                MSG_ROUTING_CONTROL,
                                params.target_extension_id);
  OpenChannelImpl(params);
}

}  // namespace extensions
