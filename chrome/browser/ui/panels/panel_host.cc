// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

using content::UserMetricsAction;

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
      MSG_ROUTING_NONE, NULL));
  chrome::SetViewType(web_contents_.get(), chrome::VIEW_TYPE_PANEL);
  web_contents_->SetDelegate(this);
  content::WebContentsObserver::Observe(web_contents_.get());

  favicon_tab_helper_.reset(new FaviconTabHelper(web_contents_.get()));
  prefs_tab_helper_.reset(new PrefsTabHelper(web_contents_.get()));

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
}

void PanelHost::DestroyWebContents() {
  favicon_tab_helper_.reset();
  prefs_tab_helper_.reset();
  web_contents_.reset();
}

gfx::Image PanelHost::GetPageIcon() const {
  return favicon_tab_helper_.get() ?
      favicon_tab_helper_->GetFavicon() : gfx::Image();
}

content::WebContents* PanelHost::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // These dispositions aren't really navigations.
  if (params.disposition == SUPPRESS_OPEN ||
      params.disposition == SAVE_TO_DISK ||
      params.disposition == IGNORE_ACTION)
    return NULL;

  // Only allow clicks on links.
  if (params.transition != content::PAGE_TRANSITION_LINK)
    return NULL;

  // Force all links to open in a new tab.
  chrome::NavigateParams navigate_params(profile_,
                                         params.url,
                                         params.transition);
  navigate_params.disposition = params.disposition == NEW_BACKGROUND_TAB ?
      params.disposition : NEW_FOREGROUND_TAB;
  chrome::Navigate(&navigate_params);
  return navigate_params.target_contents ?
      navigate_params.target_contents->web_contents() : NULL;
}

void PanelHost::NavigationStateChanged(const content::WebContents* source,
                                       unsigned changed_flags) {
  // Only need to update the title if the title changed while not loading,
  // because the title is also updated when loading state changes.
  if ((changed_flags & content::INVALIDATE_TYPE_TAB) ||
      ((changed_flags & content::INVALIDATE_TYPE_TITLE) &&
       !source->IsLoading()))
    panel_->UpdateTitleBar();
}

void PanelHost::AddNewContents(content::WebContents* source,
                               content::WebContents* new_contents,
                               WindowOpenDisposition disposition,
                               const gfx::Rect& initial_pos,
                               bool user_gesture,
                               bool* was_blocked) {
  chrome::NavigateParams navigate_params(profile_, new_contents->GetURL(),
                                         content::PAGE_TRANSITION_LINK);
  // Create a TabContents because the NavigateParams takes a TabContents,
  // not a WebContents, for the target_contents.
  TabContents* new_tab_contents = TabContents::FromWebContents(new_contents);
  if (!new_tab_contents)
    new_tab_contents = TabContents::Factory::CreateTabContents(new_contents);
  navigate_params.target_contents = new_tab_contents;

  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  navigate_params.disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;

  navigate_params.window_bounds = initial_pos;
  navigate_params.user_gesture = user_gesture;
  navigate_params.extension_app_id = panel_->extension_id();
  chrome::Navigate(&navigate_params);
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

void PanelHost::ContentsZoomChange(bool zoom_in) {
  Zoom(zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}

bool PanelHost::IsApplication() const {
  return true;
}

bool PanelHost::HandleContextMenu(const content::ContextMenuParams& params) {
  return true;  // Disallow context menu.
}

void PanelHost::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return panel_->HandleKeyboardEvent(event);
}

void PanelHost::WebContentsFocused(content::WebContents* contents) {
  panel_->WebContentsFocused(contents);
}

void PanelHost::ResizeDueToAutoResize(content::WebContents* web_contents,
                                      const gfx::Size& new_size) {
  panel_->OnContentsAutoResized(new_size);
}

void PanelHost::RenderViewCreated(content::RenderViewHost* render_view_host) {
  extensions::WindowController* window = GetExtensionWindowController();
  render_view_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
      render_view_host->GetRoutingID(), window->GetWindowId()));
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

extensions::WindowController* PanelHost::GetExtensionWindowController() const {
  return panel_->extension_window_controller();
}

content::WebContents* PanelHost::GetAssociatedWebContents() const {
  return web_contents_.get();
}

void PanelHost::Reload() {
  content::RecordAction(UserMetricsAction("Reload"));
  web_contents_->GetController().Reload(true);
}

void PanelHost::ReloadIgnoringCache() {
  content::RecordAction(UserMetricsAction("ReloadIgnoringCache"));
  web_contents_->GetController().ReloadIgnoringCache(true);
}

void PanelHost::StopLoading() {
  content::RecordAction(UserMetricsAction("Stop"));
  web_contents_->Stop();
}

void PanelHost::Zoom(content::PageZoom zoom) {
  chrome_page_zoom::Zoom(web_contents_.get(), zoom);
}
