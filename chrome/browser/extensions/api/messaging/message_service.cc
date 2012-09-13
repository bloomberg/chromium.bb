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
#include "chrome/browser/extensions/api/messaging/extension_message_port.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
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
#include "content/public/browser/browser_thread.h"
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

struct MessageService::MessageChannel {
  scoped_ptr<MessagePort> opener;
  scoped_ptr<MessagePort> receiver;
};

struct MessageService::OpenChannelParams {
  content::RenderProcessHost* source;
  std::string tab_json;
  scoped_ptr<MessagePort> receiver;
  int receiver_port_id;
  std::string source_extension_id;
  std::string target_extension_id;
  std::string channel_name;

  // Takes ownership of receiver.
  OpenChannelParams(content::RenderProcessHost* source,
                    const std::string& tab_json,
                    MessagePort* receiver,
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

static content::RenderProcessHost* GetExtensionProcess(
    Profile* profile, const std::string& extension_id) {
  SiteInstance* site_instance =
      profile->GetExtensionProcessManager()->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(extension_id));

  if (!site_instance->HasProcess())
    return NULL;

  return site_instance->GetProcess();
}

}  // namespace

content::RenderProcessHost*
    MessageService::MessagePort::GetRenderProcessHost() {
  return NULL;
}

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
    : lazy_background_task_queue_(queue),
      weak_factory_(this) {
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
  MessagePort* receiver = new ExtensionMessagePort(
      GetExtensionProcess(profile, target_extension_id), MSG_ROUTING_CONTROL,
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

  OpenChannelParams* params = new OpenChannelParams(source, tab_json, receiver,
                                                    receiver_port_id,
                                                    source_extension_id,
                                                    target_extension_id,
                                                    channel_name);

  // The target might be a lazy background page. In that case, we have to check
  // if it is loaded and ready, and if not, queue up the task and load the
  // page.
  if (MaybeAddPendingOpenChannelTask(profile, params)) {
    return;
  }

  OpenChannelImpl(scoped_ptr<OpenChannelParams>(params));
}

void MessageService::OpenChannelToNativeApp(
    int source_process_id,
    int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& native_app_name,
    const std::string& channel_name,
    const std::string& connect_message) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

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

  scoped_ptr<MessageChannel> channel(new MessageChannel());
  channel->opener.reset(new ExtensionMessagePort(source, MSG_ROUTING_CONTROL,
                                                 source_extension_id));

  NativeMessageProcessHost::MessageType type =
      channel_name == "chrome.extension.sendNativeMessage" ?
      NativeMessageProcessHost::TYPE_SEND_MESSAGE_REQUEST :
      NativeMessageProcessHost::TYPE_CONNECT;

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&NativeMessageProcessHost::Create,
                 base::WeakPtr<NativeMessageProcessHost::Client>(
                    weak_factory_.GetWeakPtr()),
                 native_app_name, connect_message, receiver_port_id,
                 type,
                 base::Bind(&MessageService::FinalizeOpenChannelToNativeApp,
                            weak_factory_.GetWeakPtr(),
                            receiver_port_id,
                            channel_name,
                            base::Passed(&channel),
                            tab_json)));
}

void MessageService::FinalizeOpenChannelToNativeApp(
    int receiver_port_id,
    const std::string& channel_name,
    scoped_ptr<MessageChannel> channel,
    const std::string& tab_json,
    NativeMessageProcessHost::ScopedHost native_process) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Abandon the channel
  if (!native_process.get()) {
    LOG(ERROR) << "Failed to create native process.";
    return;
  }
  channel->receiver.reset(new NativeMessagePort(native_process.release()));

  // Keep the opener alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();

  AddChannel(channel.release(), receiver_port_id);
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
  scoped_ptr<MessagePort> receiver;
  if (ExtensionTabUtil::GetTabById(tab_id, profile, true,
                                   NULL, NULL, &contents, NULL)) {
    receiver.reset(new ExtensionMessagePort(
        contents->web_contents()->GetRenderProcessHost(),
        contents->web_contents()->GetRenderViewHost()->GetRoutingID(),
        extension_id));
  }

  if (contents && contents->web_contents()->GetController().NeedsReload()) {
    // The tab isn't loaded yet. Don't attempt to connect. Treat this as a
    // disconnect.
    ExtensionMessagePort port(source, MSG_ROUTING_CONTROL, extension_id);
    port.DispatchOnDisconnect(GET_OPPOSITE_PORT_ID(receiver_port_id), true);
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

  scoped_ptr<OpenChannelParams> params(new OpenChannelParams(source, tab_json,
                                                             receiver.release(),
                                                             receiver_port_id,
                                                             extension_id,
                                                             extension_id,
                                                             channel_name));
  OpenChannelImpl(params.Pass());
}

bool MessageService::OpenChannelImpl(scoped_ptr<OpenChannelParams> params) {
  if (!params->source)
    return false;  // Closed while in flight.

  if (!params->receiver.get() || !params->receiver->GetRenderProcessHost()) {
    // Treat it as a disconnect.
    ExtensionMessagePort port(params->source, MSG_ROUTING_CONTROL, "");
    port.DispatchOnDisconnect(GET_OPPOSITE_PORT_ID(params->receiver_port_id),
                              true);
    return false;
  }

  // Add extra paranoid CHECKs, since we have crash reports of this being NULL.
  // http://code.google.com/p/chromium/issues/detail?id=19067
  CHECK(params->receiver->GetRenderProcessHost());

  MessageChannel* channel(new MessageChannel);
  channel->opener.reset(new ExtensionMessagePort(params->source,
                                                 MSG_ROUTING_CONTROL,
                                                 params->source_extension_id));
  channel->receiver.reset(params->receiver.release());

  CHECK(channel->receiver->GetRenderProcessHost());

  AddChannel(channel, params->receiver_port_id);

  CHECK(channel->receiver->GetRenderProcessHost());

  // Send the connect event to the receiver.  Give it the opener's port ID (the
  // opener has the opposite port ID).
  channel->receiver->DispatchOnConnect(params->receiver_port_id,
                                       params->channel_name, params->tab_json,
                                       params->source_extension_id,
                                       params->target_extension_id);

  // Keep both ends of the channel alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();
  channel->receiver->IncrementLazyKeepaliveCount();
  return true;
}

void MessageService::AddChannel(MessageChannel* channel, int receiver_port_id) {
  int channel_id = GET_CHANNEL_ID(receiver_port_id);
  CHECK(channels_.find(channel_id) == channels_.end());
  channels_[channel_id] = channel;
  pending_channels_.erase(channel_id);
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
                     weak_factory_.GetWeakPtr(), port_id, connection_error));
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
    MessagePort* port = IS_OPENER_PORT_ID(closing_port_id) ?
        channel->receiver.get() : channel->opener.get();
    port->DispatchOnDisconnect(GET_OPPOSITE_PORT_ID(closing_port_id),
                               connection_error);
  }

  // Balance the IncrementLazyKeepaliveCount() in OpenChannelImpl.
  channel->opener->DecrementLazyKeepaliveCount();
  channel->receiver->DecrementLazyKeepaliveCount();

  delete channel_iter->second;
  channels_.erase(channel_iter);
}

void MessageService::PostMessage(
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
                     weak_factory_.GetWeakPtr(), source_port_id, message));
    }
    return;
  }

  // Figure out which port the ID corresponds to.
  int dest_port_id = GET_OPPOSITE_PORT_ID(source_port_id);
  MessagePort* port = IS_OPENER_PORT_ID(dest_port_id) ?
      iter->second->opener.get() : iter->second->receiver.get();

  port->DispatchOnMessage(message, dest_port_id);
}

void MessageService::PostMessageFromNativeProcess(int port_id,
                                                  const std::string& message) {
  PostMessage(port_id, message);
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

    content::RenderProcessHost* opener_process =
        current->second->opener->GetRenderProcessHost();
    content::RenderProcessHost* receiver_process =
        current->second->receiver->GetRenderProcessHost();

    bool notify_other_port = opener_process &&
        opener_process == receiver_process;

    if (opener_process == process) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first),
                      false, notify_other_port);
    } else if (receiver_process == process) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first),
                      false, notify_other_port);
    }
  }
}

bool MessageService::MaybeAddPendingOpenChannelTask(
    Profile* profile,
    OpenChannelParams* params) {
  ExtensionService* service = profile->GetExtensionService();
  const std::string& extension_id = params->target_extension_id;
  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (extension && extension->has_lazy_background_page()) {
    // If the extension uses spanning incognito mode, make sure we're always
    // using the original profile since that is what the extension process
    // will use.
    if (!extension->incognito_split_mode())
      profile = profile->GetOriginalProfile();

    if (lazy_background_task_queue_->ShouldEnqueueTask(profile, extension)) {
      pending_channels_[GET_CHANNEL_ID(params->receiver_port_id)] =
          PendingChannel(profile, extension_id);
      scoped_ptr<OpenChannelParams> scoped_params(params);
      lazy_background_task_queue_->AddPendingTask(profile, extension_id,
          base::Bind(&MessageService::PendingOpenChannel,
                     weak_factory_.GetWeakPtr(), base::Passed(&scoped_params),
                     params->source->GetID()));
      return true;
    }
  }

  return false;
}

void MessageService::PendingOpenChannel(scoped_ptr<OpenChannelParams> params,
                                        int source_process_id,
                                        ExtensionHost* host) {
  if (!host)
    return;  // TODO(mpcomplete): notify source of disconnect?

  // Re-lookup the source process since it may no longer be valid.
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

  params->source = source;
  params->receiver.reset(new ExtensionMessagePort(host->render_process_host(),
                                                  MSG_ROUTING_CONTROL,
                                                  params->target_extension_id));
  OpenChannelImpl(params.Pass());
}

}  // namespace extensions
