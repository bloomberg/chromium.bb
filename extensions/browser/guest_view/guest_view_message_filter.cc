// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_message_filter.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/guest_view/guest_view_messages.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

namespace extensions {

GuestViewMessageFilter::GuestViewMessageFilter(int render_process_id,
                                               BrowserContext* context)
    : BrowserMessageFilter(GuestViewMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GuestViewMessageFilter::~GuestViewMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void GuestViewMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case GuestViewHostMsg_AttachGuest::ID:
    case GuestViewHostMsg_CreateMimeHandlerViewGuest::ID:
    case GuestViewHostMsg_ResizeGuest::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void GuestViewMessageFilter::OnDestruct() const {
  // Destroy the filter on the IO thread since that's where its weak pointers
  // are being used.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool GuestViewMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GuestViewMessageFilter, message)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_AttachGuest, OnAttachGuest)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_CreateMimeHandlerViewGuest,
                        OnCreateMimeHandlerViewGuest)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GuestViewMessageFilter::OnAttachGuest(
    int element_instance_id,
    int guest_instance_id,
    const base::DictionaryValue& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  manager->AttachGuest(render_process_id_,
                       element_instance_id,
                       guest_instance_id,
                       params);
}

void GuestViewMessageFilter::OnCreateMimeHandlerViewGuest(
    int render_frame_id,
    const std::string& view_id,
    int element_instance_id,
    const gfx::Size& element_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  auto rfh = RenderFrameHost::FromID(render_process_id_, render_frame_id);
  auto embedder_web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!embedder_web_contents)
    return;

  GuestViewManager::WebContentsCreatedCallback callback =
      base::Bind(&GuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback,
                 this,
                 element_instance_id,
                 render_process_id_,
                 render_frame_id,
                 element_size);

  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kViewId, view_id);
  create_params.SetInteger(guestview::kElementWidth, element_size.width());
  create_params.SetInteger(guestview::kElementHeight, element_size.height());
  manager->CreateGuest(MimeHandlerViewGuest::Type,
                       embedder_web_contents,
                       create_params,
                       callback);
}

void GuestViewMessageFilter::OnResizeGuest(int render_frame_id,
                                           int element_instance_id,
                                           const gfx::Size& new_size) {
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  auto guest_web_contents =
      manager->GetGuestByInstanceID(render_process_id_, element_instance_id);
  auto mhvg = MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  if (!mhvg)
    return;

  SetSizeParams set_size_params;
  set_size_params.enable_auto_size.reset(new bool(false));
  set_size_params.normal_size.reset(new gfx::Size(new_size));
  mhvg->SetSize(set_size_params);
}

void GuestViewMessageFilter::MimeHandlerViewGuestCreatedCallback(
    int element_instance_id,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    const gfx::Size& element_size,
    WebContents* web_contents) {
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager)
    return;

  auto guest_view = MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;

  int guest_instance_id = guest_view->guest_instance_id();
  auto rfh = RenderFrameHost::FromID(embedder_render_process_id,
                                     embedder_render_frame_id);
  if (!rfh)
    return;

  base::DictionaryValue attach_params;
  attach_params.SetInteger(guestview::kElementWidth, element_size.width());
  attach_params.SetInteger(guestview::kElementHeight, element_size.height());
  manager->AttachGuest(embedder_render_process_id,
                       element_instance_id,
                       guest_instance_id,
                       attach_params);

  rfh->Send(
      new GuestViewMsg_CreateMimeHandlerViewGuestACK(element_instance_id));
}

}  // namespace extensions
