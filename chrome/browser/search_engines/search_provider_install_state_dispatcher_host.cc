// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_state_dispatcher_host.h"

#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "ipc/ipc_message.h"
#include "googleurl/src/gurl.h"

SearchProviderInstallStateDispatcherHost::
SearchProviderInstallStateDispatcherHost(
    ResourceMessageFilter* ipc_sender,
    Profile* profile,
    int render_process_id)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        reply_with_provider_install_state_factory_(this)),
      provider_data_(profile->GetWebDataService(Profile::EXPLICIT_ACCESS),
                     NotificationType::RENDERER_PROCESS_TERMINATED,
                     Source<RenderProcessHost>(
                         RenderProcessHost::FromID(render_process_id))),
      ipc_sender_(ipc_sender),
      is_off_the_record_(profile->IsOffTheRecord()) {
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here. Instead do it lazily when required.
  DCHECK(ipc_sender);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

SearchProviderInstallStateDispatcherHost::
~SearchProviderInstallStateDispatcherHost() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

void SearchProviderInstallStateDispatcherHost::Send(IPC::Message* message) {
  ipc_sender_->Send(message);
}

bool SearchProviderInstallStateDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SearchProviderInstallStateDispatcherHost, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetSearchProviderInstallState,
                                    OnMsgGetSearchProviderInstallState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ViewHostMsg_GetSearchProviderInstallState_Params
SearchProviderInstallStateDispatcherHost::GetSearchProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host) {
  GURL requested_origin = requested_host.GetOrigin();

  // Do the security check before any others to avoid information leaks.
  if (page_location.GetOrigin() != requested_origin)
    return ViewHostMsg_GetSearchProviderInstallState_Params::Denied();

  // In incognito mode, no search information is exposed. (This check must be
  // done after the security check or else a web site can detect that the
  // user is in incognito mode just by doing a cross origin request.)
  if (is_off_the_record_)
      return ViewHostMsg_GetSearchProviderInstallState_Params::NotInstalled();

  switch (provider_data_.GetInstallState(requested_origin)) {
    case SearchProviderInstallData::NOT_INSTALLED:
      return ViewHostMsg_GetSearchProviderInstallState_Params::
          NotInstalled();

    case SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT:
      return ViewHostMsg_GetSearchProviderInstallState_Params::
          InstallButNotDefault();

    case SearchProviderInstallData::INSTALLED_AS_DEFAULT:
      return ViewHostMsg_GetSearchProviderInstallState_Params::
          InstalledAsDefault();
  }

  NOTREACHED();
  return ViewHostMsg_GetSearchProviderInstallState_Params::
      NotInstalled();
}

void
SearchProviderInstallStateDispatcherHost::OnMsgGetSearchProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    IPC::Message* reply_msg) {
  provider_data_.CallWhenLoaded(
      reply_with_provider_install_state_factory_.NewRunnableMethod(
          &SearchProviderInstallStateDispatcherHost::
          ReplyWithProviderInstallState,
          page_location,
          requested_host,
          reply_msg));
}

void SearchProviderInstallStateDispatcherHost::ReplyWithProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);
  ViewHostMsg_GetSearchProviderInstallState_Params install_state =
      GetSearchProviderInstallState(page_location, requested_host);

  ViewHostMsg_GetSearchProviderInstallState::WriteReplyParams(
      reply_msg,
      install_state);
  Send(reply_msg);
}
