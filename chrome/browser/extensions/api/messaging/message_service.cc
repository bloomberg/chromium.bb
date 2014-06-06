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
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/completion_callback.h"
#include "url/gurl.h"

using content::BrowserContext;
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

const char kReceivingEndDoesntExistError[] =
    "Could not establish connection. Receiving end does not exist.";
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kMissingPermissionError[] =
    "Access to native messaging requires nativeMessaging permission.";
const char kProhibitedByPoliciesError[] =
    "Access to the native messaging host was disabled by the system "
    "administrator.";
#endif

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
  bool include_tls_channel_id;
  std::string tls_channel_id;

  // Takes ownership of receiver.
  OpenChannelParams(content::RenderProcessHost* source,
                    scoped_ptr<base::DictionaryValue> source_tab,
                    MessagePort* receiver,
                    int receiver_port_id,
                    const std::string& source_extension_id,
                    const std::string& target_extension_id,
                    const GURL& source_url,
                    const std::string& channel_name,
                    bool include_tls_channel_id)
      : source(source),
        receiver(receiver),
        receiver_port_id(receiver_port_id),
        source_extension_id(source_extension_id),
        target_extension_id(target_extension_id),
        source_url(source_url),
        channel_name(channel_name),
        include_tls_channel_id(include_tls_channel_id) {
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
    BrowserContext* context,
    const std::string& extension_id) {
  SiteInstance* site_instance =
      ExtensionSystem::Get(context)->process_manager()->GetSiteInstanceForURL(
          Extension::GetBaseURLFromExtensionId(extension_id));
  return site_instance->HasProcess() ? site_instance->GetProcess() : NULL;
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
  DCHECK_EQ(GET_OPPOSITE_PORT_ID(port1_id), port2_id);
  DCHECK_EQ(GET_OPPOSITE_PORT_ID(port2_id), port1_id);
  DCHECK_EQ(GET_CHANNEL_ID(port1_id), GET_CHANNEL_ID(port2_id));
  DCHECK_EQ(GET_CHANNEL_ID(port1_id), channel_id);
  DCHECK_EQ(GET_CHANNEL_OPENER_ID(channel_id), port1_id);
  DCHECK_EQ(GET_CHANNEL_RECEIVERS_ID(channel_id), port2_id);

  *port1 = port1_id;
  *port2 = port2_id;
}

MessageService::MessageService(BrowserContext* context)
    : lazy_background_task_queue_(
          ExtensionSystem::Get(context)->lazy_background_task_queue()),
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

static base::LazyInstance<BrowserContextKeyedAPIFactory<MessageService> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MessageService>*
MessageService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
MessageService* MessageService::Get(BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MessageService>::Get(context);
}

void MessageService::OpenChannelToExtension(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const GURL& source_url,
    const std::string& channel_name,
    bool include_tls_channel_id) {
  content::RenderProcessHost* source =
      content::RenderProcessHost::FromID(source_process_id);
  if (!source)
    return;
  BrowserContext* context = source->GetBrowserContext();

  ExtensionSystem* extension_system = ExtensionSystem::Get(context);
  DCHECK(extension_system);
  const Extension* target_extension = extension_system->extension_service()->
      extensions()->GetByID(target_extension_id);
  if (!target_extension) {
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  // Only running ephemeral apps can receive messages. Idle cached ephemeral
  // apps are invisible and should not be connectable.
  if (util::IsEphemeralApp(target_extension_id, context) &&
      util::IsExtensionIdle(target_extension_id, context)) {
    DispatchOnDisconnect(
        source, receiver_port_id, kReceivingEndDoesntExistError);
    return;
  }

  bool is_web_connection = false;

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
        is_web_connection = true;
        is_externally_connectable =
            externally_connectable->matches.MatchesURL(source_url);
        // Only include the TLS channel ID for externally connected web pages.
        include_tls_channel_id &=
            is_externally_connectable &&
            externally_connectable->accepts_tls_channel_id;
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

  WebContents* source_contents = tab_util::GetWebContentsByID(
      source_process_id, source_routing_id);

  if (context->IsOffTheRecord() &&
      !util::IsIncognitoEnabled(target_extension_id, context)) {
    // Give the user a chance to accept an incognito connection from the web if
    // they haven't already, with the conditions:
    // - Only for spanning-mode incognito. We don't want the complication of
    //   spinning up an additional process here which might need to do some
    //   setup that we're not expecting.
    // - Only for extensions that can't normally be enabled in incognito, since
    //   that surface (e.g. chrome://extensions) should be the only one for
    //   enabling in incognito. In practice this means platform apps only.
    if (!is_web_connection || IncognitoInfo::IsSplitMode(target_extension) ||
        target_extension->can_be_incognito_enabled() ||
        // This check may show a dialog.
        !IncognitoConnectability::Get(context)
             ->Query(target_extension, source_contents, source_url)) {
      DispatchOnDisconnect(
          source, receiver_port_id, kReceivingEndDoesntExistError);
      return;
    }
  }

  // Note: we use the source's profile here. If the source is an incognito
  // process, we will use the incognito EPM to find the right extension process,
  // which depends on whether the extension uses spanning or split mode.
  MessagePort* receiver = new ExtensionMessagePort(
      GetExtensionProcess(context, target_extension_id),
      MSG_ROUTING_CONTROL,
      target_extension_id);

  // Include info about the opener's tab (if it was a tab).
  scoped_ptr<base::DictionaryValue> source_tab;
  GURL source_url_for_tab;

  if (source_contents && ExtensionTabUtil::GetTabId(source_contents) >= 0) {
    // Only the tab id is useful to platform apps for internal use. The
    // unnecessary bits will be stripped out in
    // MessagingBindings::DispatchOnConnect().
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
                                                    channel_name,
                                                    include_tls_channel_id);

  // If the target requests the TLS channel id, begin the lookup for it.
  // The target might also be a lazy background page, checked next, but the
  // loading of lazy background pages continues asynchronously, so enqueue
  // messages awaiting TLS channel ID first.
  if (include_tls_channel_id) {
    pending_tls_channel_id_channels_[GET_CHANNEL_ID(params->receiver_port_id)]
        = PendingMessagesQueue();
    property_provider_.GetDomainBoundCert(
        Profile::FromBrowserContext(context),
        source_url,
        base::Bind(&MessageService::GotDomainBoundCert,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(make_scoped_ptr(params))));
    return;
  }

  // The target might be a lazy background page. In that case, we have to check
  // if it is loaded and ready, and if not, queue up the task and load the
  // page.
  if (MaybeAddPendingLazyBackgroundPageOpenChannelTask(
          context, target_extension, params)) {
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
    has_permission = extension &&
                     extension->permissions_data()->HasAPIPermission(
                         APIPermission::kNativeMessaging);
  }

  if (!has_permission) {
    DispatchOnDisconnect(source, receiver_port_id, kMissingPermissionError);
    return;
  }

  PrefService* pref_service = profile->GetPrefs();

  // Verify that the host is not blocked by policies.
  NativeMessageProcessHost::PolicyPermission policy_permission =
      NativeMessageProcessHost::IsHostAllowed(pref_service, native_app_name);
  if (policy_permission == NativeMessageProcessHost::DISALLOW) {
    DispatchOnDisconnect(source, receiver_port_id, kProhibitedByPoliciesError);
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
          source_extension_id, native_app_name, receiver_port_id,
          policy_permission == NativeMessageProcessHost::ALLOW_ALL);

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
  const char kNativeMessagingNotSupportedError[] =
      "Native Messaging is not supported on this platform.";
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
        channel_name,
        false));  // Connections to tabs don't get TLS channel IDs.
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
                                       params->source_url,
                                       params->tls_channel_id);

  // Keep both ends of the channel alive until the channel is closed.
  channel->opener->IncrementLazyKeepaliveCount();
  channel->receiver->IncrementLazyKeepaliveCount();
  return true;
}

void MessageService::AddChannel(MessageChannel* channel, int receiver_port_id) {
  int channel_id = GET_CHANNEL_ID(receiver_port_id);
  CHECK(channels_.find(channel_id) == channels_.end());
  channels_[channel_id] = channel;
  pending_lazy_background_page_channels_.erase(channel_id);
}

void MessageService::CloseChannel(int port_id,
                                  const std::string& error_message) {
  // Note: The channel might be gone already, if the other side closed first.
  int channel_id = GET_CHANNEL_ID(port_id);
  MessageChannelMap::iterator it = channels_.find(channel_id);
  if (it == channels_.end()) {
    PendingLazyBackgroundPageChannelMap::iterator pending =
        pending_lazy_background_page_channels_.find(channel_id);
    if (pending != pending_lazy_background_page_channels_.end()) {
      lazy_background_task_queue_->AddPendingTask(
          pending->second.first, pending->second.second,
          base::Bind(&MessageService::PendingLazyBackgroundPageCloseChannel,
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

void MessageService::PostMessage(int source_port_id, const Message& message) {
  int channel_id = GET_CHANNEL_ID(source_port_id);
  MessageChannelMap::iterator iter = channels_.find(channel_id);
  if (iter == channels_.end()) {
    // If this channel is pending, queue up the PostMessage to run once
    // the channel opens.
    EnqueuePendingMessage(source_port_id, channel_id, message);
    return;
  }

  DispatchMessage(source_port_id, iter->second, message);
}

void MessageService::PostMessageFromNativeProcess(int port_id,
                                                  const std::string& message) {
  PostMessage(port_id, Message(message, false /* user_gesture */));
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

void MessageService::EnqueuePendingMessage(int source_port_id,
                                           int channel_id,
                                           const Message& message) {
  PendingTlsChannelIdMap::iterator pending_for_tls_channel_id =
      pending_tls_channel_id_channels_.find(channel_id);
  if (pending_for_tls_channel_id != pending_tls_channel_id_channels_.end()) {
    pending_for_tls_channel_id->second.push_back(
        PendingMessage(source_port_id, message));
    // Pending messages must only be pending the TLS channel ID or lazy
    // background page loading, never both.
    return;
  }
  EnqueuePendingMessageForLazyBackgroundLoad(source_port_id,
                                             channel_id,
                                             message);
}

void MessageService::EnqueuePendingMessageForLazyBackgroundLoad(
    int source_port_id,
    int channel_id,
    const Message& message) {
  PendingLazyBackgroundPageChannelMap::iterator pending =
      pending_lazy_background_page_channels_.find(channel_id);
  if (pending != pending_lazy_background_page_channels_.end()) {
    lazy_background_task_queue_->AddPendingTask(
        pending->second.first, pending->second.second,
        base::Bind(&MessageService::PendingLazyBackgroundPagePostMessage,
                   weak_factory_.GetWeakPtr(), source_port_id, message));
  }
}

void MessageService::DispatchMessage(int source_port_id,
                                     MessageChannel* channel,
                                     const Message& message) {
  // Figure out which port the ID corresponds to.
  int dest_port_id = GET_OPPOSITE_PORT_ID(source_port_id);
  MessagePort* port = IS_OPENER_PORT_ID(dest_port_id) ?
      channel->opener.get() : channel->receiver.get();

  port->DispatchOnMessage(message, dest_port_id);
}

bool MessageService::MaybeAddPendingLazyBackgroundPageOpenChannelTask(
    BrowserContext* context,
    const Extension* extension,
    OpenChannelParams* params) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return false;

  // If the extension uses spanning incognito mode, make sure we're always
  // using the original profile since that is what the extension process
  // will use.
  if (!IncognitoInfo::IsSplitMode(extension))
    context = ExtensionsBrowserClient::Get()->GetOriginalContext(context);

  if (!lazy_background_task_queue_->ShouldEnqueueTask(context, extension))
    return false;

  pending_lazy_background_page_channels_
      [GET_CHANNEL_ID(params->receiver_port_id)] =
          PendingLazyBackgroundPageChannel(context, extension->id());
  scoped_ptr<OpenChannelParams> scoped_params(params);
  lazy_background_task_queue_->AddPendingTask(
      context,
      extension->id(),
      base::Bind(&MessageService::PendingLazyBackgroundPageOpenChannel,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&scoped_params),
                 params->source->GetID()));
  return true;
}

void MessageService::GotDomainBoundCert(scoped_ptr<OpenChannelParams> params,
                                        const std::string& tls_channel_id) {
  params->tls_channel_id.assign(tls_channel_id);
  int channel_id = GET_CHANNEL_ID(params->receiver_port_id);

  PendingTlsChannelIdMap::iterator pending_for_tls_channel_id =
      pending_tls_channel_id_channels_.find(channel_id);
  if (pending_for_tls_channel_id == pending_tls_channel_id_channels_.end()) {
    NOTREACHED();
    return;
  }

  BrowserContext* context = params->source->GetBrowserContext();

  const Extension* target_extension =
      ExtensionSystem::Get(context)->extension_service()->extensions()->GetByID(
          params->target_extension_id);
  if (!target_extension) {
    pending_tls_channel_id_channels_.erase(channel_id);
    DispatchOnDisconnect(
        params->source, params->receiver_port_id,
        kReceivingEndDoesntExistError);
    return;
  }
  PendingMessagesQueue& pending_messages = pending_for_tls_channel_id->second;
  if (MaybeAddPendingLazyBackgroundPageOpenChannelTask(
          context, target_extension, params.get())) {
    // Lazy background queue took ownership. Release ours.
    ignore_result(params.release());
    // Messages queued up waiting for the TLS channel ID now need to be queued
    // up for the lazy background page to load.
    for (PendingMessagesQueue::iterator it = pending_messages.begin();
         it != pending_messages.end();
         it++) {
      EnqueuePendingMessageForLazyBackgroundLoad(it->first, channel_id,
                                                 it->second);
    }
  } else {
    OpenChannelImpl(params.Pass());
    // Messages queued up waiting for the TLS channel ID can be posted now.
    MessageChannelMap::iterator channel_iter = channels_.find(channel_id);
    if (channel_iter != channels_.end()) {
      for (PendingMessagesQueue::iterator it = pending_messages.begin();
           it != pending_messages.end();
           it++) {
        DispatchMessage(it->first, channel_iter->second, it->second);
      }
    }
  }
  pending_tls_channel_id_channels_.erase(channel_id);
}

void MessageService::PendingLazyBackgroundPageOpenChannel(
    scoped_ptr<OpenChannelParams> params,
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
