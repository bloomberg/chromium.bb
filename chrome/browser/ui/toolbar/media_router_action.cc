// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/media_router_action_platform_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using media_router::MediaRouterDialogController;

MediaRouterAction::MediaRouterAction(Browser* browser)
    : media_router::IssuesObserver(GetMediaRouter(browser)),
      media_router::MediaRoutesObserver(GetMediaRouter(browser)),
      id_("media_router_action"),
      name_(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE)),
      media_router_active_icon_(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(IDR_MEDIA_ROUTER_ACTIVE_ICON)),
      media_router_error_icon_(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(IDR_MEDIA_ROUTER_ERROR_ICON)),
      media_router_idle_icon_(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(IDR_MEDIA_ROUTER_IDLE_ICON)),
      media_router_warning_icon_(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(IDR_MEDIA_ROUTER_WARNING_ICON)),
      current_icon_(&media_router_idle_icon_),
      has_local_route_(false),
      delegate_(nullptr),
      platform_delegate_(MediaRouterActionPlatformDelegate::Create(browser)),
      contextual_menu_(browser) {
}

MediaRouterAction::~MediaRouterAction() {
}

const std::string& MediaRouterAction::GetId() const {
  return id_;
}

void MediaRouterAction::SetDelegate(ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;
}

gfx::Image MediaRouterAction::GetIcon(content::WebContents* web_contents,
                                      const gfx::Size& size) {
  return *current_icon_;
}

base::string16 MediaRouterAction::GetActionName() const {
  return name_;
}

base::string16 MediaRouterAction::GetAccessibleName(
    content::WebContents* web_contents) const {
  return GetTooltip(web_contents);
}

base::string16 MediaRouterAction::GetTooltip(
    content::WebContents* web_contents) const {
  return name_;
}

bool MediaRouterAction::IsEnabled(
    content::WebContents* web_contents) const {
  return true;
}

bool MediaRouterAction::WantsToRun(
    content::WebContents* web_contents) const {
  return false;
}

bool MediaRouterAction::HasPopup(
    content::WebContents* web_contents) const {
  return true;
}

void MediaRouterAction::HidePopup() {
  GetMediaRouterDialogController()->HideMediaRouterDialog();
}

gfx::NativeView MediaRouterAction::GetPopupNativeView() {
  return nullptr;
}

ui::MenuModel* MediaRouterAction::GetContextMenu() {
  return contextual_menu_.menu_model();
}

bool MediaRouterAction::CanDrag() const {
  return false;
}

bool MediaRouterAction::ExecuteAction(bool by_user) {
  GetMediaRouterDialogController()->ShowMediaRouterDialog();
  if (platform_delegate_)
    platform_delegate_->CloseOverflowMenuIfOpen();
  return true;
}

void MediaRouterAction::UpdateState() {
}

bool MediaRouterAction::DisabledClickOpensMenu() const {
  return false;
}

void MediaRouterAction::OnIssueUpdated(const media_router::Issue* issue) {
  issue_.reset(issue ? new media_router::Issue(*issue) : nullptr);

  MaybeUpdateIcon();
}

void MediaRouterAction::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  has_local_route_ =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                      return route.is_local(); }) !=
      routes.end();
  MaybeUpdateIcon();
}

MediaRouterDialogController*
MediaRouterAction::GetMediaRouterDialogController() {
  DCHECK(delegate_);
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  DCHECK(web_contents);
  return MediaRouterDialogController::GetOrCreateForWebContents(web_contents);
}

media_router::MediaRouter* MediaRouterAction::GetMediaRouter(Browser* browser) {
  return media_router::MediaRouterFactory::GetApiForBrowserContext(
      static_cast<content::BrowserContext*>(browser->profile()));
}

void MediaRouterAction::MaybeUpdateIcon() {
  const gfx::Image* new_icon = GetCurrentIcon();

  // Update the current state if it has changed.
  if (new_icon != current_icon_) {
    current_icon_ = new_icon;

    // Tell the associated view to update its icon to reflect the change made
    // above.
    if (delegate_)
      delegate_->UpdateState();
  }
}

const gfx::Image* MediaRouterAction::GetCurrentIcon() const {
  // Highest priority is to indicate whether there's an issue.
  if (issue_) {
    if (issue_->severity() == media_router::Issue::FATAL)
      return &media_router_error_icon_;
    if (issue_->severity() == media_router::Issue::WARNING)
      return &media_router_warning_icon_;
  }

  return has_local_route_ ?
      &media_router_active_icon_ : &media_router_idle_icon_;
}
