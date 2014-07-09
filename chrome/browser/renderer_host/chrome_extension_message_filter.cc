// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_extension_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/message_bundle.h"

using content::BrowserThread;
using extensions::APIPermission;

namespace {

const uint32 kFilteredMessageClasses[] = {
  ChromeMsgStart,
  ExtensionMsgStart,
};

// Logs an action to the extension activity log for the specified profile.  Can
// be called from any thread.
void AddActionToExtensionActivityLog(
    Profile* profile,
    scoped_refptr<extensions::Action> action) {
  // The ActivityLog can only be accessed from the main (UI) thread.  If we're
  // running on the wrong thread, re-dispatch from the main thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AddActionToExtensionActivityLog, profile, action));
  } else {
    if (!g_browser_process->profile_manager()->IsValidProfile(profile))
      return;
    // If the action included a URL, check whether it is for an incognito
    // profile.  The check is performed here so that it can safely be done from
    // the UI thread.
    if (action->page_url().is_valid() || !action->page_title().empty())
      action->set_page_incognito(profile->IsOffTheRecord());
    extensions::ActivityLog* activity_log =
        extensions::ActivityLog::GetInstance(profile);
    activity_log->LogAction(action);
  }
}

}  // namespace

ChromeExtensionMessageFilter::ChromeExtensionMessageFilter(
    int render_process_id,
    Profile* profile)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      profile_(profile),
      extension_info_map_(
          extensions::ExtensionSystem::Get(profile)->info_map()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile));
}

ChromeExtensionMessageFilter::~ChromeExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool ChromeExtensionMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardRead,
                        OnCanTriggerClipboardRead)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardWrite,
                        OnCanTriggerClipboardWrite)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToNativeApp,
                        OnOpenChannelToNativeApp)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ExtensionHostMsg_GetMessageBundle,
                                    OnGetExtMessageBundle)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CloseChannel, OnExtensionCloseChannel)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddAPIActionToActivityLog,
                        OnAddAPIActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddDOMActionToActivityLog,
                        OnAddDOMActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddEventToActivityLog,
                        OnAddEventToExtensionActivityLog);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_CloseChannel::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ChromeExtensionMessageFilter::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }
}

void ChromeExtensionMessageFilter::OnCanTriggerClipboardRead(
    const GURL& origin, bool* allowed) {
  *allowed = extension_info_map_->SecurityOriginHasAPIPermission(
      origin, render_process_id_, APIPermission::kClipboardRead);
}

void ChromeExtensionMessageFilter::OnCanTriggerClipboardWrite(
    const GURL& origin, bool* allowed) {
  // Since all extensions could historically write to the clipboard, preserve it
  // for compatibility.
  *allowed = (origin.SchemeIs(extensions::kExtensionScheme) ||
      extension_info_map_->SecurityOriginHasAPIPermission(
          origin, render_process_id_, APIPermission::kClipboardWrite));
}

void ChromeExtensionMessageFilter::OnOpenChannelToExtension(
    int routing_id,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    bool include_tls_channel_id,
    int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ChromeExtensionMessageFilter::OpenChannelToExtensionOnUIThread,
          this, render_process_id_, routing_id, port2_id, info,
          channel_name, include_tls_channel_id));
}

void ChromeExtensionMessageFilter::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    bool include_tls_channel_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_) {
    extensions::MessageService::Get(profile_)
        ->OpenChannelToExtension(source_process_id,
                                 source_routing_id,
                                 receiver_port_id,
                                 info.source_id,
                                 info.target_id,
                                 info.source_url,
                                 channel_name,
                                 include_tls_channel_id);
  }
}

void ChromeExtensionMessageFilter::OnOpenChannelToNativeApp(
    int routing_id,
    const std::string& source_extension_id,
    const std::string& native_app_name,
    int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ChromeExtensionMessageFilter::OpenChannelToNativeAppOnUIThread,
          this, routing_id, port2_id, source_extension_id, native_app_name));
}

void ChromeExtensionMessageFilter::OpenChannelToNativeAppOnUIThread(
    int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& native_app_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_) {
    extensions::MessageService::Get(profile_)
        ->OpenChannelToNativeApp(render_process_id_,
                                 source_routing_id,
                                 receiver_port_id,
                                 source_extension_id,
                                 native_app_name);
  }
}

void ChromeExtensionMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeExtensionMessageFilter::OpenChannelToTabOnUIThread,
                 this, render_process_id_, routing_id, port2_id, tab_id,
                 extension_id, channel_name));
}

void ChromeExtensionMessageFilter::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    int tab_id,
    const std::string& extension_id,
    const std::string& channel_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_) {
    extensions::MessageService::Get(profile_)
        ->OpenChannelToTab(source_process_id,
                           source_routing_id,
                           receiver_port_id,
                           tab_id,
                           extension_id,
                           channel_name);
  }
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  const extensions::Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  base::FilePath extension_path;
  std::string default_locale;
  if (extension) {
    extension_path = extension->path();
    default_locale = extensions::LocaleInfo::GetDefaultLocale(extension);
  }

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(
          &ChromeExtensionMessageFilter::OnGetExtMessageBundleOnBlockingPool,
          this, extension_path, extension_id, default_locale, reply_msg));
}

void ChromeExtensionMessageFilter::OnGetExtMessageBundleOnBlockingPool(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  scoped_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
      extensions::file_util::LoadMessageBundleSubstitutionMap(
          extension_path, extension_id, default_locale));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                      *dictionary_map);
  Send(reply_msg);
}

void ChromeExtensionMessageFilter::OnExtensionCloseChannel(
    int port_id,
    const std::string& error_message) {
  if (!content::RenderProcessHost::FromID(render_process_id_))
    return;  // To guard against crash in browser_tests shutdown.

  extensions::MessageService* message_service =
      extensions::MessageService::Get(profile_);
  if (message_service)
    message_service->CloseChannel(port_id, error_message);
}

void ChromeExtensionMessageFilter::OnAddAPIActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_CALL,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(
        activity_log_constants::kActionExtra, params.extra);
  }
  AddActionToExtensionActivityLog(profile_, action);
}

void ChromeExtensionMessageFilter::OnAddDOMActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_DOMAction_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_DOM_ACCESS,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  action->set_page_url(params.url);
  action->set_page_title(base::UTF16ToUTF8(params.url_title));
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      params.call_type);
  AddActionToExtensionActivityLog(profile_, action);
}

void ChromeExtensionMessageFilter::OnAddEventToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_EVENT,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(activity_log_constants::kActionExtra,
                                       params.extra);
  }
  AddActionToExtensionActivityLog(profile_, action);
}

void ChromeExtensionMessageFilter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
  profile_ = NULL;
}
