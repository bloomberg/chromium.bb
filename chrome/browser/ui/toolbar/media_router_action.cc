// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using media_router::MediaRouterDialogController;

MediaRouterAction::MediaRouterAction()
    : id_("media_router_action"),
      name_(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE)),
      media_router_idle_icon_(ui::ResourceBundle::GetSharedInstance().
          GetImageNamed(IDR_MEDIA_ROUTER_IDLE_ICON)),
      delegate_(nullptr) {
}

MediaRouterAction::~MediaRouterAction() {
}

const std::string& MediaRouterAction::GetId() const {
  return id_;
}

void MediaRouterAction::SetDelegate(ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;
}

gfx::Image MediaRouterAction::GetIcon(
    content::WebContents* web_contents) {
  // TODO(apacible): Return icon based on casting state.
  return media_router_idle_icon_;
}

gfx::ImageSkia MediaRouterAction::GetIconWithBadge() {
  DCHECK(delegate_);
  return *GetIcon(delegate_->GetCurrentWebContents()).ToImageSkia();
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
  GetMediaRouterDialogController()->CloseMediaRouterDialog();
}

gfx::NativeView MediaRouterAction::GetPopupNativeView() {
  return nullptr;
}

ui::MenuModel* MediaRouterAction::GetContextMenu() {
  return nullptr;
}

bool MediaRouterAction::CanDrag() const {
  return false;
}

bool MediaRouterAction::ExecuteAction(bool by_user) {
  GetMediaRouterDialogController()->ShowMediaRouterDialog();
  return true;
}

void MediaRouterAction::UpdateState() {
}

MediaRouterDialogController*
MediaRouterAction::GetMediaRouterDialogController() {
  DCHECK(delegate_);
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  DCHECK(web_contents);
  return MediaRouterDialogController::GetOrCreateForWebContents(web_contents);
}
