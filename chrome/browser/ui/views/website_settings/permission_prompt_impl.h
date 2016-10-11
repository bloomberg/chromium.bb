// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_PROMPT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_PROMPT_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/permission_prompt.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

class Browser;
class PermissionsBubbleDialogDelegateView;
class Profile;

class PermissionPromptImpl : public PermissionPrompt {
 public:
  explicit PermissionPromptImpl(Browser* browser);
  ~PermissionPromptImpl() override;

  // PermissionPrompt:
  void SetDelegate(Delegate* delegate) override;
  void Show(const std::vector<PermissionRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  bool CanAcceptRequestUpdate() override;
  void Hide() override;
  bool IsVisible() override;
  void UpdateAnchorPosition() override;
  gfx::NativeWindow GetNativeWindow() override;

  void Closing();
  void ToggleAccept(int index, bool value);
  void TogglePersist(bool value);
  void Accept();
  void Deny();

  Profile* GetProfile();

 private:
  // These three functions have separate implementations for Views-based and
  // Cocoa-based browsers, to allow this bubble to be used in either.

  // Returns the view to anchor the permission bubble to. May be null.
  views::View* GetAnchorView();
  // Returns the anchor point to anchor the permission bubble to, as a fallback.
  // Only used if GetAnchorView() returns nullptr.
  gfx::Point GetAnchorPoint();
  // Returns the type of arrow to display on the permission bubble.
  views::BubbleBorder::Arrow GetAnchorArrow();

  Browser* browser_;
  Delegate* delegate_;
  PermissionsBubbleDialogDelegateView* bubble_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_PROMPT_IMPL_H_
