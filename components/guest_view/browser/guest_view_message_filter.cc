// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_message_filter.h"

#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

namespace guest_view {

GuestViewMessageFilter::GuestViewMessageFilter(int render_process_id,
                                               BrowserContext* context)
    : BrowserMessageFilter(GuestViewMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GuestViewMessageFilter::GuestViewMessageFilter(
    const uint32* message_classes_to_filter,
    size_t num_message_classes_to_filter,
    int render_process_id,
    BrowserContext* context)
    : BrowserMessageFilter(message_classes_to_filter,
                           num_message_classes_to_filter),
      render_process_id_(render_process_id),
      browser_context_(context),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GuestViewMessageFilter::~GuestViewMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

GuestViewManager* GuestViewMessageFilter::GetOrCreateGuestViewManager() {
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context_,
        scoped_ptr<GuestViewManagerDelegate>(new GuestViewManagerDelegate()));
  }
  return manager;
}

void GuestViewMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == GuestViewMsgStart)
    *thread = BrowserThread::UI;
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
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_ViewGarbageCollected,
                        OnViewGarbageCollected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GuestViewMessageFilter::OnViewGarbageCollected(int view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetOrCreateGuestViewManager()->ViewGarbageCollected(render_process_id_,
                                                      view_instance_id);
}

void GuestViewMessageFilter::OnAttachGuest(
    int element_instance_id,
    int guest_instance_id,
    const base::DictionaryValue& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto manager = GuestViewManager::FromBrowserContext(browser_context_);
  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  if (!manager)
    return;

  manager->AttachGuest(render_process_id_,
                       element_instance_id,
                       guest_instance_id,
                       params);
}

}  // namespace guest_view
