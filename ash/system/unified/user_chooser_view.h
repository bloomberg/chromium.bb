// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_USER_CHOOSER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_USER_CHOOSER_VIEW_H_

#include "ash/media_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class UnifiedSystemTrayController;

// Circular image view with user's icon of |user_index|.
views::View* CreateUserAvatarView(int user_index);

// Get accessibility string for |user_index|.
base::string16 GetUserItemAccessibleString(int user_index);

// A button item of a switchable user.
class UserItemButton : public views::Button, public views::ButtonListener {
 public:
  UserItemButton(int user_index,
                 UnifiedSystemTrayController* controller,
                 bool has_close_button);
  ~UserItemButton() override = default;

  void SetCaptureState(mojom::MediaCaptureState capture_state);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  const int user_index_;
  UnifiedSystemTrayController* const controller_;
  views::ImageView* const capture_icon_;

  DISALLOW_COPY_AND_ASSIGN(UserItemButton);
};

// A detailed view of user chooser.
class UserChooserView : public views::View, public MediaCaptureObserver {
 public:
  UserChooserView(UnifiedSystemTrayController* controller);
  ~UserChooserView() override;

  // MediaCaptureObserver:
  void OnMediaCaptureChanged(
      const std::vector<mojom::MediaCaptureState>& capture_states) override;

 private:
  std::vector<UserItemButton*> user_item_buttons_;

  DISALLOW_COPY_AND_ASSIGN(UserChooserView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
