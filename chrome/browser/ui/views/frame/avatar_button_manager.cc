// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/avatar_button_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"

AvatarButtonManager::AvatarButtonManager(BrowserNonClientFrameView* frame_view)
    : frame_view_(frame_view), view_(nullptr) {}

void AvatarButtonManager::Update(AvatarButtonStyle style) {
  BrowserView* browser_view = frame_view_->browser_view();
  BrowserFrame* frame = frame_view_->frame();
  Profile* profile = browser_view->browser()->profile();

  // This should never be called in incognito mode.
  DCHECK(browser_view->IsRegularOrGuestSession());
  ProfileAttributesEntry* unused;
  if ((browser_view->IsBrowserTypeNormal() &&
       // Tests may not have a profile manager.
       g_browser_process->profile_manager() &&
       g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &unused)) ||
      // Desktop guest shows the avatar button.
      browser_view->IsIncognito()) {
    if (!view_) {
      view_ = new NewAvatarButton(this, style, profile);
      view_->set_id(VIEW_ID_AVATAR_BUTTON);
      frame_view_->AddChildView(view_);
      frame->GetRootView()->Layout();
    }
  } else if (view_) {
    delete view_;
    view_ = nullptr;
    frame->GetRootView()->Layout();
  }
}

void AvatarButtonManager::ButtonPreferredSizeChanged() {
  // Perform a re-layout if the avatar button has changed, since that can affect
  // the size of the tabs.
  if (!view_ || !frame_view_->browser_view()->initialized())
    return;  // Ignore the update during view creation.

  frame_view_->InvalidateLayout();
  frame_view_->frame()->GetRootView()->Layout();
}

void AvatarButtonManager::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(view_, sender);
  BrowserWindow::AvatarBubbleMode mode =
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT;
  if ((event.IsMouseEvent() &&
       static_cast<const ui::MouseEvent&>(event).IsRightMouseButton()) ||
      (event.type() == ui::ET_GESTURE_LONG_PRESS)) {
    mode = BrowserWindow::AVATAR_BUBBLE_MODE_FAST_USER_SWITCH;
  }
  frame_view_->browser_view()->ShowAvatarBubbleFromAvatarButton(
      mode, signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN, false);
}
