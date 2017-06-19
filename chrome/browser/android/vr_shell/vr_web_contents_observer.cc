// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_web_contents_observer.h"

#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

namespace vr_shell {

VrWebContentsObserver::VrWebContentsObserver(content::WebContents* web_contents,
                                             UiInterface* ui_interface,
                                             VrShell* vr_shell)
    : WebContentsObserver(web_contents),
      ui_interface_(ui_interface),
      vr_shell_(vr_shell) {
  ui_interface_->SetURL(web_contents->GetVisibleURL());
  DidChangeVisibleSecurityState();
}

VrWebContentsObserver::~VrWebContentsObserver() {}

void VrWebContentsObserver::SetUiInterface(UiInterface* ui_interface) {
  ui_interface_ = ui_interface;
}

void VrWebContentsObserver::DidStartLoading() {
  ui_interface_->SetLoading(true);
}

void VrWebContentsObserver::DidStopLoading() {
  ui_interface_->SetLoading(false);
}

void VrWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    ui_interface_->SetURL(navigation_handle->GetURL());
  }
}

void VrWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    ui_interface_->SetURL(navigation_handle->GetURL());
  }
}

void VrWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    ui_interface_->SetURL(navigation_handle->GetURL());
  }
}

void VrWebContentsObserver::DidChangeVisibleSecurityState() {
  const auto* helper = SecurityStateTabHelper::FromWebContents(web_contents());
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  bool malware = (security_info.malicious_content_status !=
                  security_state::MALICIOUS_CONTENT_STATUS_NONE);
  ui_interface_->SetSecurityInfo(security_info.security_level, malware);
}

void VrWebContentsObserver::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  vr_shell_->OnFullscreenChanged(entered_fullscreen);
}

void VrWebContentsObserver::WebContentsDestroyed() {
  vr_shell_->ContentWebContentsDestroyed();
}

void VrWebContentsObserver::WasHidden() {
  vr_shell_->ContentWasHidden();
}

void VrWebContentsObserver::WasShown() {
  vr_shell_->ContentWasShown();
}

void VrWebContentsObserver::MainFrameWasResized(bool width_changed) {
  vr_shell_->ContentFrameWasResized(width_changed);
}

void VrWebContentsObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  new_host->GetWidget()->GetView()->SetIsInVR(true);
}

}  // namespace vr_shell
