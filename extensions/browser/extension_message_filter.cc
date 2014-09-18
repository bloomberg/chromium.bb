// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "extensions/browser/blob_holder.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context),
      extension_info_map_(ExtensionSystem::Get(context)->info_map()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ExtensionMessageFilter::~ExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_AttachGuest::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_AddLazyListener::ID:
    case ExtensionHostMsg_CreateMimeHandlerViewGuest::ID:
    case ExtensionHostMsg_RemoveLazyListener::ID:
    case ExtensionHostMsg_AddFilteredListener::ID:
    case ExtensionHostMsg_RemoveFilteredListener::ID:
    case ExtensionHostMsg_ShouldSuspendAck::ID:
    case ExtensionHostMsg_SuspendAck::ID:
    case ExtensionHostMsg_TransferBlobsAck::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ExtensionMessageFilter::OnDestruct() const {
  // Destroy the filter on the IO thread since that's where its weak pointers
  // are being used.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener,
                        OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyListener,
                        OnExtensionAddLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AttachGuest,
                        OnExtensionAttachGuest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CreateMimeHandlerViewGuest,
                        OnExtensionCreateMimeHandlerViewGuest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyListener,
                        OnExtensionRemoveLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddFilteredListener,
                        OnExtensionAddFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveFilteredListener,
                        OnExtensionRemoveFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ShouldSuspendAck,
                        OnExtensionShouldSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_SuspendAck,
                        OnExtensionSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_TransferBlobsAck,
                        OnExtensionTransferBlobsAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_GenerateUniqueID,
                        OnExtensionGenerateUniqueID)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ResumeRequests,
                        OnExtensionResumeRequests);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestForIOThread,
                        OnExtensionRequestForIOThread)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    router->AddEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    router->AddEventListenerForURL(event_name, process, listener_url);
  } else {
    NOTREACHED() << "Tried to add an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    router->RemoveEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    router->RemoveEventListenerForURL(event_name, process, listener_url);
  } else {
    NOTREACHED() << "Tried to remove an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionAddLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;
  router->AddLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAttachGuest(
    int routing_id,
    int element_instance_id,
    int guest_instance_id,
    const base::DictionaryValue& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GuestViewManager* manager =
      GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  manager->AttachGuest(render_process_id_,
                       routing_id,
                       element_instance_id,
                       guest_instance_id,
                       params);
}

void ExtensionMessageFilter::OnExtensionCreateMimeHandlerViewGuest(
    int render_frame_id,
    const std::string& src,
    const std::string& mime_type,
    int element_instance_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GuestViewManager* manager =
      GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id);
  content::WebContents* embedder_web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!embedder_web_contents)
    return;

  GuestViewManager::WebContentsCreatedCallback callback =
      base::Bind(&ExtensionMessageFilter::MimeHandlerViewGuestCreatedCallback,
                 this,
                 element_instance_id,
                 render_process_id_,
                 render_frame_id,
                 src);
  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kMimeType, mime_type);
  create_params.SetString(mime_handler_view::kSrc, src);
  manager->CreateGuest(MimeHandlerViewGuest::Type,
                       "",
                       embedder_web_contents,
                       create_params,
                       callback);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;
  router->RemoveLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;
  router->AddFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionRemoveFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;
  router->RemoveFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionShouldSuspendAck(
     const std::string& extension_id, int sequence_id) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context_)->process_manager();
  if (process_manager)
    process_manager->OnShouldSuspendAck(extension_id, sequence_id);
}

void ExtensionMessageFilter::OnExtensionSuspendAck(
     const std::string& extension_id) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context_)->process_manager();
  if (process_manager)
    process_manager->OnSuspendAck(extension_id);
}

void ExtensionMessageFilter::OnExtensionTransferBlobsAck(
    const std::vector<std::string>& blob_uuids) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  BlobHolder::FromRenderProcessHost(process)->DropBlobs(blob_uuids);
}

void ExtensionMessageFilter::OnExtensionGenerateUniqueID(int* unique_id) {
  static int next_unique_id = 0;
  *unique_id = ++next_unique_id;
}

void ExtensionMessageFilter::OnExtensionResumeRequests(int route_id) {
  content::ResourceDispatcherHost::Get()->ResumeBlockedRequestsForRoute(
      render_process_id_, route_id);
}

void ExtensionMessageFilter::OnExtensionRequestForIOThread(
    int routing_id,
    const ExtensionHostMsg_Request_Params& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ExtensionFunctionDispatcher::DispatchOnIOThread(
      extension_info_map_.get(),
      browser_context_,
      render_process_id_,
      weak_ptr_factory_.GetWeakPtr(),
      routing_id,
      params);
}

void ExtensionMessageFilter::MimeHandlerViewGuestCreatedCallback(
    int element_instance_id,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    const std::string& src,
    content::WebContents* web_contents) {
  GuestViewManager* manager =
      GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  MimeHandlerViewGuest* guest_view =
      MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  int guest_instance_id = guest_view->guest_instance_id();

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      embedder_render_process_id, embedder_render_frame_id);
  if (!rfh)
    return;

  base::DictionaryValue attach_params;
  attach_params.SetString(mime_handler_view::kSrc, src);
  manager->AttachGuest(embedder_render_process_id,
                       rfh->GetRenderViewHost()->GetRoutingID(),
                       element_instance_id,
                       guest_instance_id,
                       attach_params);

  IPC::Message* msg =
      new ExtensionMsg_CreateMimeHandlerViewGuestACK(element_instance_id);
  msg->set_routing_id(rfh->GetRoutingID());
  rfh->Send(msg);
}

}  // namespace extensions
