// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/avatar_button_manager.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif  // !defined(OS_CHROMEOS)

AvatarButtonManager::AvatarButtonManager(BrowserNonClientFrameView* frame_view)
#if !defined(OS_CHROMEOS)
    : frame_view_(frame_view)
#endif  // defined(OS_CHROMEOS)
{
}

void AvatarButtonManager::Update(AvatarButtonStyle style) {
#if defined(OS_CHROMEOS)
  DCHECK_EQ(style, AvatarButtonStyle::NONE);
#else
  BrowserView* browser_view = frame_view_->browser_view();
  BrowserFrame* frame = frame_view_->frame();
  Profile* profile = browser_view->browser()->profile();

  // This should never be called in incognito mode.
  DCHECK(browser_view->IsRegularOrGuestSession());
  ProfileAttributesEntry* unused;
  if (style != AvatarButtonStyle::NONE &&
      ((browser_view->IsBrowserTypeNormal() &&
        // Tests may not have a profile manager.
        g_browser_process->profile_manager() &&
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath(), &unused)) ||
       // Desktop guest shows the avatar button.
       browser_view->IsIncognito())) {
    if (!view_) {
      view_ = new AvatarButton(this, style, profile, this);
      view_->set_id(VIEW_ID_AVATAR_BUTTON);
      frame_view_->AddChildView(view_);
      frame->GetRootView()->Layout();
    }
  } else if (view_) {
    delete view_;
    view_ = nullptr;
    frame->GetRootView()->Layout();
  }
#endif  // defined(OS_CHROMEOS)
}

void AvatarButtonManager::OnMenuButtonClicked(views::MenuButton* sender,
                                              const gfx::Point& point,
                                              const ui::Event* event) {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  DCHECK_EQ(view_, sender);
  BrowserWindow::AvatarBubbleMode mode =
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT;
  if ((event->IsMouseEvent() && event->AsMouseEvent()->IsRightMouseButton()) ||
      (event->type() == ui::ET_GESTURE_LONG_PRESS)) {
    return;
  }
  frame_view_->browser_view()->ShowAvatarBubbleFromAvatarButton(
      mode, signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN, false);
  view_->OnAvatarButtonPressed(event);
#endif  // defined(OS_CHROMEOS)
}
