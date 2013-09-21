// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/message_service.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"

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

namespace {
const char kReceivingEndDoesntExistError[] =
    "Could not establish connection. Receiving end does not exist.";
const char kMissingPermissionError[] =
    "Access to native messaging requires nativeMessaging permission.";
const char kNativeMessagingNotSupportedError[] =
    "Native Messaging is not supported on this platform.";
}

struct MessageService::MessageChannel {
  scoped_ptr<MessagePort> opener;
  scoped_ptr<MessagePort> receiver;
};

struct MessageService::OpenChannelParams {
  content::RenderProcessHost* source;
  base::DictionaryValue source_tab;
  scoped_ptr<MessagePort> receiver;
  int receiver_port_id;
  std::string source_extension_id;
  std::string target_extension_id;
  GURL source_url;
  std::string channel_name;

  // Takes ownership of receiver.
  OpenChannelParams(content::RenderProcessHost* source,
                    scoped_ptr<base::DictionaryValue> source_tab,
                    MessagePort* receiver,
                    int receiver_port_id,
                    const std::string& source_extension_id,
                    const std::string& target_extension_id,
                    const GURL& source_url,
                    const std::string& channel_name)
      : source(source),
        receiver(receiver),
        receiver_port_id(receiver_port_id),
        source_extension_id(source_extension_id),
        target_extension_id(target_extension_id),
        source_url(source_url),
        channel_name(channel_name) {
    if (source_tab)
      this->source_tab.Swap(source_tab.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenChannelParams);
};

namespace {

static base::StaticAtomicSequenceNumber g_next_channel_id;
static base::StaticAtomicSequenceNumber g_channel_id_overflow_count;

static content::RenderProcessHost* GetExtensionProcess(
    Profile* profile, const std::string& extension_id) {
  SiteInstance* site_instance =
      ExtensionSystem::Get(profile)->process_manager()->
          GetSiteInstanceForURL(
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
  unsigned channel_id =
      static_cast<unsigned>(g_next_channel_id.GetNext()) % (kint32max/2);

  if (channel_id == 0) {
    int overflow_count = g_channel_id_overflow_count.GetNext();
    if (overflow_count > 0)
      UMA_HISTOGRAM_BOOLEAN("Extensions.AllocatePortIdPairOverflow", true);
  }

  unsigned port1_id = channel_id * 2;
  unsigned port2_id = channel_id * 2 + 1;

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

MessageService::MessageService(Profile* profile)
    : lazy_background_task_queue_(
          ExtensionSystem::Get(profile)->lazy_background_task_queue()),
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

static base::LazyInstance<ProfileKeyedAPIFactory<MessageService> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<MessageService>* MessageService::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
MessageService* MessageService::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<MessageService>::GetForProfile(profile);
}

void MessageService::OpenChannelToExtension(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const GURL& source_url,
    const std::string& channel_name) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

  const Extension* target_extension = ExtensionSystem::Get(profile)->
      extension_service()->extensions()->GetByID(target_extension_id);
  if (!target_extension) {
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();

  if (profile->IsOffTheRecord() &&
      !extension_service->IsIncognitoEnabled(target_extension_id)) {
    // Allow the security token apps (normal, dev) to be connectable from
    // incognito profiles. See http://crbug.com/295845.
    std::set<std::string> incognito_whitelist;
    incognito_whitelist.insert("E4FCC42F7C7776C0985996DAED74F630C4F0A785");
    incognito_whitelist.insert("D3D12919F7F00FE553E8A573AAA7147C51DD65C9");
    if (!extensions::SimpleFeature::IsIdInWhitelist(target_extension_id,
                                                    incognito_whitelist)) {
      DispatchOnDisconnect(
          source, receiver_port_id, kReceivingEndDoesntExistError);
      return;
    }
  }

  if (source_extension_id != target_extension_id) {
    // It's an external connection. Check the externally_connectable manifest
    // key if it's present. If it's not, we allow connection from any extension
    // but not webpages.
    ExternallyConnectableInfo* externally_connectable =
        static_cast<ExternallyConnectableInfo*>(
            target_extension->GetManifestData(
                manifest_keys::kExternallyConnectable));
    bool is_externally_connectable = false;

    if (externally_connectable) {
      if (source_extension_id.empty()) {
        // No source extension ID so the source was a web page. Check that the
        // URL matches.
        is_externally_connectable =
            externally_connectable->matches.MatchesURL(source_url);
      } else {
        // Source extension ID so the source was an extension. Check that the
        // extension matches.
        is_externally_connectable =
            externally_connectable->IdCanConnect(source_extension_id);
      }
    } else {
      // Default behaviour. Any extension, no webpages.
      is_externally_connectable = !source_extension_id.empty();
    }

    if (!is_externally_connectable) {
      // Important: use kReceivingEndDoesntExistError here so that we don't
      // leak information about this extension to callers. This way it's
      // indistinguishable from the extension just not existing.
      DispatchOnDisconnect(
          source, receiver_port_id, kReceivingEndDoesntExistError);
      return;
    }
  }

  // Note: we use the source's profile here. If the source is an incognito
  // process, we will use the incognito EPM to find the right extension process,
  // which depends on whether the extension uses spanning or split mode.
  MessagePort* receiver = new ExtensionMessagePort(
      GetExtensionProcess(profile, target_extension_id), MSG_ROUTING_CONTROL,
      target_extension_id);
  WebContents* source_contents = tab_util::GetWebContentsByID(
      source_process_id, source_routing_id);

  // Include info about the opener's tab (if it was a tab).
  scoped_ptr<base::DictionaryValue> source_tab;
  GURL source_url_for_tab;

  if (source_contents && ExtensionTabUtil::GetTabId(source_contents) >= 0) {
    // Platform apps can be sent messages, but don't have a Tab concept.
    if (!target_extension->is_platform_app())
      source_tab.reset(ExtensionTabUtil::CreateTabValue(source_contents));
    source_url_for_tab = source_url;
  }

  OpenChannelParams* params = new OpenChannelParams(source,
                                                    source_tab.Pass(),
                                                    receiver,
                                                    receiver_port_id,
                                                    source_extension_id,
                                                    target_extension_id,
                                                    source_url_for_tab,
                                                    channel_name);

  // The target might be a lazy background page. In that case, we have to check
  // if it is loaded and ready, and if not, queue up the task and load the
  // page.
  if (MaybeAddPendingOpenChannelTask(profile, target_extension, params)) {
    return;
  }

  OpenChannelImpl(make_scoped_ptr(params));
}

void MessageService::OpenChannelToNativeApp(
    int source_process_id,
    int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& native_app_name) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  bool has_permission = false;
  if (extension_service) {
    const Extension* extension =
        extension_service->GetExtensionById(source_extension_id, false);
    has_permission = extension && extension->HasAPIPermission(
        APIPermission::kNativeMessaging);
  }

  if (!has_permission) {
    DispatchOnDisconnect(source, receiver_port_id, kMissingPermissionError);
    return;
  }

  scoped_ptr<MessageChannel> channel(new MessageChannel());
  channel->opener.reset(new ExtensionMessagePort(source, MSG_ROUTING_CONTROL,
                                                 source_extension_id));

  // Get handle of the native view and pass it to the native messaging host.
  gfx::NativeView native_view =
      content::RenderWidgetHost::FromID(source_process_id, source_routing_id)->
          GetView()->GetNativeView();

  scoped_ptr<NativeMessageProcessHost> native_process =
      NativeMessageProcessHost::Create(
          native_view,
          base::WeakPtr<NativeMessageProcessHost::Client>(
              weak_factory_.GetWeakPtr()),
          source_extension_id, native_app_name, receiver_port_id);

  // Abandon the channel.
  if (!native_process.get()) {
    LOG(ERROR) << "Failed to create native process.";
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }
  channel->receiver.reset(new NativeMessagePort(native_process.release()));

  // Keep the opener alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();

  AddChannel(channel.release(), receiver_port_id);
#else  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
  DispatchOnDisconnect(
      source, receiver_port_id, kNativeMessagingNotSupportedError);
#endif  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
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

  WebContents* contents = NULL;
  scoped_ptr<MessagePort> receiver;
  if (ExtensionTabUtil::GetTabById(tab_id, profile, true,
                                   NULL, NULL, &contents, NULL)) {
    receiver.reset(new ExtensionMessagePort(
        contents->GetRenderProcessHost(),
        contents->GetRenderViewHost()->GetRoutingID(),
        extension_id));
  }

  if (contents && contents->GetController().NeedsReload()) {
    // The tab isn't loaded yet. Don't attempt to connect.
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  scoped_ptr<OpenChannelParams> params(new OpenChannelParams(
        source,
        scoped_ptr<base::DictionaryValue>(),  // Source tab doesn't make sense
                                              // for opening to tabs.
        receiver.release(),
        receiver_port_id,
        extension_id,
        extension_id,
        GURL(),  // Source URL doesn't make sense for opening to tabs.
        channel_name));
  OpenChannelImpl(params.Pass());
}

bool MessageService::OpenChannelImpl(scoped_ptr<OpenChannelParams> params) {
  if (!params->source)
    return false;  // Closed while in flight.

  if (!params->receiver || !params->receiver->GetRenderProcessHost()) {
    DispatchOnDisconnect(params->source,
                         params->receiver_port_id,
                         kReceivingEndDoesntExistError);
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
                                       params->channel_name,
                                       params->source_tab,
                                       params->source_extension_id,
                                       params->target_extension_id,
                                       params->source_url);

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

void MessageService::CloseChannel(int port_id,
                                  const std::string& error_message) {
  // Note: The channel might be gone already, if the other side closed first.
  int channel_id = GET_CHANNEL_ID(port_id);
  MessageChannelMap::iterator it = channels_.find(channel_id);
  if (it == channels_.end()) {
    PendingChannelMap::iterator pending = pending_channels_.find(channel_id);
    if (pending != pending_channels_.end()) {
      lazy_background_task_queue_->AddPendingTask(
          pending->second.first, pending->second.second,
          base::Bind(&MessageService::PendingCloseChannel,
                     weak_factory_.GetWeakPtr(), port_id, error_message));
    }
    return;
  }
  CloseChannelImpl(it, port_id, error_message, true);
}

void MessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter,
    int closing_port_id,
    const std::string& error_message,
    bool notify_other_port) {
  MessageChannel* channel = channel_iter->second;

  // Notify the other side.
  if (notify_other_port) {
    MessagePort* port = IS_OPENER_PORT_ID(closing_port_id) ?
        channel->receiver.get() : channel->opener.get();
    port->DispatchOnDisconnect(GET_OPPOSITE_PORT_ID(closing_port_id),
                               error_message);
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

    // Only notify the other side if it has a different porocess host.
    bool notify_other_port = opener_process && receiver_process &&
        opener_process != receiver_process;

    if (opener_process == process) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first),
                       std::string(), notify_other_port);
    } else if (receiver_process == process) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first),
                       std::string(), notify_other_port);
    }
  }
}

bool MessageService::MaybeAddPendingOpenChannelTask(
    Profile* profile,
    const Extension* extension,
    OpenChannelParams* params) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return false;

  // If the extension uses spanning incognito mode, make sure we're always
  // using the original profile since that is what the extension process
  // will use.
  if (!IncognitoInfo::IsSplitMode(extension))
    profile = profile->GetOriginalProfile();

  if (!lazy_background_task_queue_->ShouldEnqueueTask(profile, extension))
    return false;

  pending_channels_[GET_CHANNEL_ID(params->receiver_port_id)] =
      PendingChannel(profile, extension->id());
  scoped_ptr<OpenChannelParams> scoped_params(params);
  lazy_background_task_queue_->AddPendingTask(profile, extension->id(),
      base::Bind(&MessageService::PendingOpenChannel,
                 weak_factory_.GetWeakPtr(), base::Passed(&scoped_params),
                 params->source->GetID()));
  return true;
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

void MessageService::DispatchOnDisconnect(content::RenderProcessHost* source,
                                          int port_id,
                                          const std::string& error_message) {
  ExtensionMessagePort port(source, MSG_ROUTING_CONTROL, "");
  port.DispatchOnDisconnect(GET_OPPOSITE_PORT_ID(port_id), error_message);
}

}  // namespace extensions
