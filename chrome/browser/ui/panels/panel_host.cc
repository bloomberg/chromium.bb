// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_host.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

PanelHost::PanelHost(Panel* panel, Profile* profile)
    : panel_(panel),
      profile_(profile),
      extension_function_dispatcher_(profile, this),
      weak_factory_(this) {
}

PanelHost::~PanelHost() {
}

void PanelHost::Init(const GURL& url) {
  if (url.is_empty())
    return;

  web_contents_.reset(content::WebContents::Create(
      profile_, content::SiteInstance::CreateForURL(profile_, url),
      MSG_ROUTING_NONE, NULL, NULL));
  chrome::SetViewType(web_contents_.get(), chrome::VIEW_TYPE_PANEL);
  web_contents_->SetDelegate(this);
  content::WebContentsObserver::Observe(web_contents_.get());

  favicon_tab_helper_.reset(new FaviconTabHelper(web_contents_.get()));

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
}

void PanelHost::DestroyWebContents() {
  web_contents_.reset();
}

SkBitmap PanelHost::GetPageIcon() const {
  return favicon_tab_helper_.get() ?
      favicon_tab_helper_->GetFavicon() : SkBitmap();
}

void PanelHost::NavigationStateChanged(const content::WebContents* source,
                                       unsigned changed_flags) {
  // Only need to update the title if the title changed while not loading,
  // because the title is also updated when loading state changes.
  if (changed_flags & content::INVALIDATE_TYPE_TITLE &&
      !source->IsLoading())
    panel_->UpdateTitleBar();
}

void PanelHost::ActivateContents(content::WebContents* contents) {
  panel_->Activate();
}

void PanelHost::DeactivateContents(content::WebContents* contents) {
  panel_->Deactivate();
}

void PanelHost::LoadingStateChanged(content::WebContents* source) {
  bool is_loading = source->IsLoading();
  panel_->LoadingStateChanged(is_loading);
}

void PanelHost::CloseContents(content::WebContents* source) {
  panel_->Close();
}

void PanelHost::MoveContents(content::WebContents* source,
                             const gfx::Rect& pos) {
  panel_->SetBounds(pos);
}

bool PanelHost::IsPopupOrPanel(const content::WebContents* source) const {
  return true;
}

bool PanelHost::IsApplication() const {
  return true;
}

void PanelHost::ResizeDueToAutoResize(content::WebContents* web_contents,
                                      const gfx::Size& new_size) {
  panel_->OnContentsAutoResized(new_size);
}

void PanelHost::RenderViewGone(base::TerminationStatus status) {
  CloseContents(web_contents_.get());
}

void PanelHost::WebContentsDestroyed(content::WebContents* web_contents) {
  // Close the panel after we return to the message loop (not immediately,
  // otherwise, it may destroy this object before the stack has a chance
  // to cleanly unwind.)
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PanelHost::ClosePanel, weak_factory_.GetWeakPtr()));
}

void PanelHost::ClosePanel() {
  panel_->Close();
}

bool PanelHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PanelHost, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PanelHost::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  if (!web_contents_.get())
    return;

  extension_function_dispatcher_.Dispatch(params,
                                          web_contents_->GetRenderViewHost());
}

ExtensionWindowController* PanelHost::GetExtensionWindowController() const {
  return panel_->extension_window_controller();
}

content::WebContents* PanelHost::GetAssociatedWebContents() const {
  return web_contents_.get();
}
