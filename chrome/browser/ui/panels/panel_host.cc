// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_host.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

using base::UserMetricsAction;

PanelHost::PanelHost(Panel* panel, Profile* profile)
    : panel_(panel),
      profile_(profile),
      weak_factory_(this) {
}

PanelHost::~PanelHost() {
}

void PanelHost::Init(const GURL& url) {
  if (url.is_empty())
    return;

  content::WebContents::CreateParams create_params(
      profile_, content::SiteInstance::CreateForURL(profile_, url));
  web_contents_.reset(content::WebContents::Create(create_params));
  extensions::SetViewType(web_contents_.get(), extensions::VIEW_TYPE_PANEL);
  web_contents_->SetDelegate(this);
  // web_contents_ may be passed to PageZoom::Zoom(), so it needs
  // a ZoomController.
  ui_zoom::ZoomController::CreateForWebContents(web_contents_.get());
  content::WebContentsObserver::Observe(web_contents_.get());

  // Needed to give the web contents a Tab ID. Extension APIs
  // expect web contents to have a Tab ID.
  SessionTabHelper::CreateForWebContents(web_contents_.get());
  SessionTabHelper::FromWebContents(web_contents_.get())->SetWindowID(
      panel_->session_id());

  favicon::CreateContentFaviconDriverForWebContents(web_contents_.get());
  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_.get());
  extensions::ExtensionWebContentsObserver::GetForWebContents(
      web_contents_.get())->dispatcher()->set_delegate(this);

  web_contents_->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
}

void PanelHost::DestroyWebContents() {
  // Cannot do a web_contents_.reset() because web_contents_.get() will
  // still return the pointer when we CHECK in WebContentsDestroyed (or if
  // we get called back in the middle of web contents destruction, which
  // WebView might do when it detects the web contents is destroyed).
  content::WebContents* contents = web_contents_.release();
  delete contents;
}

gfx::Image PanelHost::GetPageIcon() const {
  if (!web_contents_.get())
    return gfx::Image();

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());
  CHECK(favicon_driver);
  return favicon_driver->GetFavicon();
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
  if (params.transition != ui::PAGE_TRANSITION_LINK)
    return NULL;

  // Force all links to open in a new tab.
  chrome::NavigateParams navigate_params(profile_,
                                         params.url,
                                         params.transition);
  switch (params.disposition) {
    case NEW_BACKGROUND_TAB:
    case NEW_WINDOW:
    case OFF_THE_RECORD:
      navigate_params.disposition = params.disposition;
      break;
    default:
      navigate_params.disposition = NEW_FOREGROUND_TAB;
      break;
  }
  chrome::Navigate(&navigate_params);
  return navigate_params.target_contents;
}

void PanelHost::NavigationStateChanged(content::WebContents* source,
                                       content::InvalidateTypes changed_flags) {
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
                               const gfx::Rect& initial_rect,
                               bool user_gesture,
                               bool* was_blocked) {
  chrome::NavigateParams navigate_params(profile_, new_contents->GetURL(),
                                         ui::PAGE_TRANSITION_LINK);
  navigate_params.target_contents = new_contents;

  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  navigate_params.disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;

  navigate_params.window_bounds = initial_rect;
  navigate_params.user_gesture = user_gesture;
  navigate_params.extension_app_id = panel_->extension_id();
  chrome::Navigate(&navigate_params);
}

void PanelHost::ActivateContents(content::WebContents* contents) {
  panel_->Activate();
}

void PanelHost::LoadingStateChanged(content::WebContents* source,
    bool to_different_document) {
  bool is_loading = source->IsLoading() && to_different_document;
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

void PanelHost::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return panel_->HandleKeyboardEvent(event);
}

void PanelHost::ResizeDueToAutoResize(content::WebContents* web_contents,
                                      const gfx::Size& new_size) {
  panel_->OnContentsAutoResized(new_size);
}

void PanelHost::RenderProcessGone(base::TerminationStatus status) {
  CloseContents(web_contents_.get());
}

void PanelHost::WebContentsDestroyed() {
  // Web contents should only be destroyed by us.
  CHECK(!web_contents_.get());

  // Close the panel after we return to the message loop (not immediately,
  // otherwise, it may destroy this object before the stack has a chance
  // to cleanly unwind.)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PanelHost::ClosePanel, weak_factory_.GetWeakPtr()));
}

void PanelHost::ClosePanel() {
  panel_->Close();
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
  ui_zoom::PageZoom::Zoom(web_contents_.get(), zoom);
}
