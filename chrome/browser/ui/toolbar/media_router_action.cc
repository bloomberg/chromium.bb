// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action_platform_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

using media_router::MediaRouterDialogControllerImpl;

namespace {

media_router::MediaRouter* GetMediaRouter(Browser* browser) {
  return media_router::MediaRouterFactory::GetApiForBrowserContext(
      browser->profile());
}

}  // namespace

MediaRouterAction::MediaRouterAction(Browser* browser,
                                     ToolbarActionsBar* toolbar_actions_bar)
    : media_router::IssuesObserver(GetMediaRouter(browser)),
      media_router::MediaRoutesObserver(GetMediaRouter(browser)),
      current_icon_(gfx::VectorIconId::MEDIA_ROUTER_IDLE),
      has_local_display_route_(false),
      has_dialog_(false),
      delegate_(nullptr),
      browser_(browser),
      toolbar_actions_bar_(toolbar_actions_bar),
      platform_delegate_(MediaRouterActionPlatformDelegate::Create(browser)),
      contextual_menu_(browser, this),
      tab_strip_model_observer_(this),
      toolbar_actions_bar_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(browser_);
  DCHECK(toolbar_actions_bar_);
  tab_strip_model_observer_.Add(browser_->tab_strip_model());
  toolbar_actions_bar_observer_.Add(toolbar_actions_bar_);
  RegisterObserver();
}

MediaRouterAction::~MediaRouterAction() {
  UnregisterObserver();
}

std::string MediaRouterAction::GetId() const {
  return ComponentToolbarActionsFactory::kMediaRouterActionId;
}

void MediaRouterAction::SetDelegate(ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;

  // Updates the current dialog state if |delegate_| is non-null and has
  // WebContents ready.
  // In cases such as opening a new browser window, SetDelegate() will be
  // called before the WebContents is set. In those cases, we update the dialog
  // state when ActiveTabChanged() is called.
  if (delegate_ && delegate_->GetCurrentWebContents())
    UpdateDialogState();
}

gfx::Image MediaRouterAction::GetIcon(content::WebContents* web_contents,
                                      const gfx::Size& size) {
  // Color is defined in the icon.
  return gfx::Image(
      gfx::CreateVectorIcon(current_icon_, gfx::kPlaceholderColor));
}

base::string16 MediaRouterAction::GetActionName() const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE);
}

base::string16 MediaRouterAction::GetAccessibleName(
    content::WebContents* web_contents) const {
  return GetTooltip(web_contents);
}

base::string16 MediaRouterAction::GetTooltip(
    content::WebContents* web_contents) const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT);
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

bool MediaRouterAction::ExecuteAction(bool by_user) {
  base::RecordAction(base::UserMetricsAction("MediaRouter_Icon_Click"));

  if (GetMediaRouterDialogController()->IsShowingMediaRouterDialog()) {
    GetMediaRouterDialogController()->HideMediaRouterDialog();
    return false;
  }

  GetMediaRouterDialogController()->ShowMediaRouterDialog();
  if (GetPlatformDelegate()) {
    media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
        GetPlatformDelegate()->CloseOverflowMenuIfOpen() ?
        media_router::MediaRouterDialogOpenOrigin::OVERFLOW_MENU :
        media_router::MediaRouterDialogOpenOrigin::TOOLBAR);
  }
  return true;
}

void MediaRouterAction::UpdateState() {
  DCHECK(delegate_);
  delegate_->UpdateState();
}

bool MediaRouterAction::DisabledClickOpensMenu() const {
  return false;
}

void MediaRouterAction::OnIssueUpdated(const media_router::Issue* issue) {
  issue_.reset(issue ? new media_router::Issue(*issue) : nullptr);

  MaybeUpdateIcon();
}

void MediaRouterAction::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  has_local_display_route_ =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                     return route.is_local() && route.for_display();
                   }) != routes.end();
  MaybeUpdateIcon();
  MaybeRemoveAction();
}

void MediaRouterAction::ActiveTabChanged(content::WebContents* old_contents,
                                         content::WebContents* new_contents,
                                         int index,
                                         int reason) {
  UpdateDialogState();
  MaybeRemoveAction();
}

void MediaRouterAction::OnToolbarActionsBarAnimationEnded() {
  // Depress the action if the dialog is shown, release it otherwise.
  DCHECK(delegate_);
  if (GetMediaRouterDialogController()->IsShowingMediaRouterDialog())
    OnDialogShown();
}

void MediaRouterAction::OnDialogHidden() {
  if (has_dialog_) {
    has_dialog_ = false;
    DCHECK(delegate_);
    delegate_->OnPopupClosed();
  }
}

void MediaRouterAction::OnDialogShown() {
  if (!has_dialog_) {
    has_dialog_ = true;
    DCHECK(delegate_);
    // We depress the action regardless of whether ExecuteAction() was user
    // initiated.
    delegate_->OnPopupShown(true);
  }
}

void MediaRouterAction::ToggleVisibilityPreference() {
  browser_->profile()->GetPrefs()->SetBoolean(
      prefs::kMediaRouterAlwaysShowActionIcon, !ShouldAlwaysShowIcon());
  MaybeRemoveAction();
}

void MediaRouterAction::UpdateDialogState() {
  MediaRouterDialogControllerImpl* controller =
      GetMediaRouterDialogController();

  if (!controller)
    return;

  // |controller| keeps track of |this| if |this| was created with the browser
  // window or ephemerally by activating the Cast functionality. If |this| was
  // created in overflow mode, it will be destroyed when the overflow menu is
  // closed. If SetMediaRouterAction() was previously called, this is a no-op.
  if (!toolbar_actions_bar_->in_overflow_mode())
    controller->SetMediaRouterAction(weak_ptr_factory_.GetWeakPtr());

  // Update the button in case the pressed state is out of sync with dialog
  // visibility. If we just added the ephemeral icon to the toolbar and need to
  // depress it, we do that in |OnToolbarActionsBarAnimationEnded|.
  DCHECK(delegate_);
  if (ShouldAlwaysShowIcon() && controller->IsShowingMediaRouterDialog())
    OnDialogShown();
  else if (!controller->IsShowingMediaRouterDialog())
    OnDialogHidden();
}

MediaRouterDialogControllerImpl*
MediaRouterAction::GetMediaRouterDialogController() {
  DCHECK(delegate_);
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  DCHECK(web_contents);
  return MediaRouterDialogControllerImpl::GetOrCreateForWebContents(
      web_contents);
}

MediaRouterActionPlatformDelegate* MediaRouterAction::GetPlatformDelegate() {
  return platform_delegate_.get();
}

void MediaRouterAction::MaybeUpdateIcon() {
  gfx::VectorIconId new_icon = GetCurrentIcon();

  // Update the current state if it has changed.
  if (new_icon != current_icon_) {
    current_icon_ = new_icon;

    // Tell the associated view to update its icon to reflect the change made
    // above.
    UpdateState();
  }
}

void MediaRouterAction::MaybeRemoveAction() {
  if (!ShouldAlwaysShowIcon() &&
      !GetMediaRouterDialogController()->IsShowingMediaRouterDialog() &&
      !has_local_display_route_) {
    // Warning: |this| can be deleted here.
    ToolbarActionsModel::Get(browser_->profile())
        ->RemoveComponentAction(GetId());
  }
}

bool MediaRouterAction::ShouldAlwaysShowIcon() {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  return pref_service->GetBoolean(prefs::kMediaRouterAlwaysShowActionIcon);
}

gfx::VectorIconId MediaRouterAction::GetCurrentIcon() const {
  // Highest priority is to indicate whether there's an issue.
  if (issue_) {
    if (issue_->severity() == media_router::Issue::FATAL)
      return gfx::VectorIconId::MEDIA_ROUTER_ERROR;
    if (issue_->severity() == media_router::Issue::WARNING)
      return gfx::VectorIconId::MEDIA_ROUTER_WARNING;
  }

  return has_local_display_route_ ? gfx::VectorIconId::MEDIA_ROUTER_ACTIVE
                                  : gfx::VectorIconId::MEDIA_ROUTER_IDLE;
}

base::WeakPtr<MediaRouterAction> MediaRouterAction::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
