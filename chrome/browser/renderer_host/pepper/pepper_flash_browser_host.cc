// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_browser_host.h"

#include "base/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/time_conversion.h"

#ifdef OS_WIN
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreServices/CoreServices.h>
#endif

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;
using content::ResourceContext;

namespace chrome {

namespace {

// Get the ResourceContext on the UI thread for the given render process ID.
ResourceContext* GetResourceContext(int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* render_process_host = RenderProcessHost::FromID(
      render_process_id);
  if (render_process_host && render_process_host->GetBrowserContext())
    return render_process_host->GetBrowserContext()->GetResourceContext();
  return NULL;
}

}  // namespace

PepperFlashBrowserHost::PepperFlashBrowserHost(
    BrowserPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      resource_context_(NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  int unused;
  host->GetRenderViewIDsForInstance(instance, &render_process_id_, &unused);
}

PepperFlashBrowserHost::~PepperFlashBrowserHost() {
}

int32_t PepperFlashBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Flash_UpdateActivity,
                                        OnMsgUpdateActivity);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_GetLocalTimeZoneOffset,
                                      OnMsgGetLocalTimeZoneOffset);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_Flash_GetLocalDataRestrictions,
        OnMsgGetLocalDataRestrictions);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashBrowserHost::OnMsgUpdateActivity(
    ppapi::host::HostMessageContext* host_context) {
#if defined(OS_WIN)
  // Reading then writing back the same value to the screensaver timeout system
  // setting resets the countdown which prevents the screensaver from turning
  // on "for a while". As long as the plugin pings us with this message faster
  // than the screensaver timeout, it won't go on.
  int value = 0;
  if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &value, 0))
    SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, value, NULL, 0);
#elif defined(OS_MACOSX)
  UpdateSystemActivity(OverallAct);
#else
  // TODO(brettw) implement this for other platforms.
#endif
  return PP_OK;
}

int32_t PepperFlashBrowserHost::OnMsgGetLocalTimeZoneOffset(
    ppapi::host::HostMessageContext* host_context,
    const base::Time& t) {
  // The reason for this processing being in the browser process is that on
  // Linux, the localtime calls require filesystem access prohibited by the
  // sandbox.
  host_context->reply_msg = PpapiPluginMsg_Flash_GetLocalTimeZoneOffsetReply(
      ppapi::PPGetLocalTimeZoneOffset(t));
  return PP_OK;
}

int32_t PepperFlashBrowserHost::OnMsgGetLocalDataRestrictions(
    ppapi::host::HostMessageContext* context) {
  // Getting the LocalDataRestrictions needs to be done on the IO thread,
  // however it relies on the ResourceContext which can only be accessed from
  // the UI thread. We lazily initialize |resource_context_| by grabbing the
  // pointer from the UI thread and then call |GetLocalDataRestrictions| with
  // it.
  GURL document_url = host_->GetDocumentURLForInstance(pp_instance());
  GURL plugin_url = host_->GetPluginURLForInstance(pp_instance());
  if (resource_context_) {
    GetLocalDataRestrictions(context->MakeReplyMessageContext(), document_url,
                             plugin_url, resource_context_);
  } else {
    BrowserThread::PostTaskAndReplyWithResult(BrowserThread::UI, FROM_HERE,
        base::Bind(&GetResourceContext, render_process_id_),
        base::Bind(&PepperFlashBrowserHost::GetLocalDataRestrictions,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext(),
                   document_url, plugin_url));
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashBrowserHost::GetLocalDataRestrictions(
    ppapi::host::ReplyMessageContext reply_context,
    const GURL& document_url,
    const GURL& plugin_url,
    ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Note that the resource context lives on the IO thread and is owned by the
  // browser profile so its lifetime should outlast ours.
  if (!resource_context_)
    resource_context_ = resource_context;

  PP_FlashLSORestrictions restrictions = PP_FLASHLSORESTRICTIONS_NONE;
  if (resource_context_ && document_url.is_valid() && plugin_url.is_valid()) {
    content::ContentBrowserClient* client =
        content::GetContentClient()->browser();
    if (!client->AllowPluginLocalDataAccess(document_url, plugin_url,
                                            resource_context_)) {
      restrictions = PP_FLASHLSORESTRICTIONS_BLOCK;
    } else if (client->AllowPluginLocalDataSessionOnly(plugin_url,
                                                       resource_context_)) {
      restrictions = PP_FLASHLSORESTRICTIONS_IN_MEMORY;
    }
  }
  SendReply(reply_context, PpapiPluginMsg_Flash_GetLocalDataRestrictionsReply(
      static_cast<int32_t>(restrictions)));
}

}  // namespace chrome
