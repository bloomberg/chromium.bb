// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include "ash/common/wm_window.h"
#include "ash/content/shell_content_state.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "ui/gfx/image/image_skia.h"

SessionStateDelegateChromeos::SessionStateDelegateChromeos() {}

SessionStateDelegateChromeos::~SessionStateDelegateChromeos() {}

bool SessionStateDelegateChromeos::ShouldShowAvatar(
    ash::WmWindow* window) const {
  return chrome::MultiUserWindowManager::GetInstance()->ShouldShowAvatar(
      ash::WmWindow::GetAuraWindow(window));
}

gfx::ImageSkia SessionStateDelegateChromeos::GetAvatarImageForWindow(
    ash::WmWindow* window) const {
  content::BrowserContext* context =
      ash::ShellContentState::GetInstance()->GetBrowserContextForWindow(
          ash::WmWindow::GetAuraWindow(window));
  return GetAvatarImageForContext(context);
}
