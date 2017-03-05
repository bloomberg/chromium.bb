// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_
#define ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_

#include "ash/common/media_controller.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
}

namespace ash {

enum class LoginStatus;

namespace tray {

// The view displaying information about the user, such as user's avatar, email
// address, name, and more. View has no borders.
class UserCardView : public views::View, public MediaCaptureObserver {
 public:
  // |max_width| takes effect only if |login_status| is LOGGED_IN_PUBLIC.
  UserCardView(LoginStatus login_status, int max_width, int user_index);
  ~UserCardView() override;

  // View:
  void PaintChildren(const ui::PaintContext& context) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // MediaCaptureObserver:
  void OnMediaCaptureChanged(
      const std::vector<mojom::MediaCaptureState>& capture_states) override;

 private:
  // Creates the content for the public mode.
  void AddPublicModeUserContent(int max_width);

  void AddUserContent(views::BoxLayout* layout, LoginStatus login_status);

  bool is_active_user() const { return !user_index_; }

  int user_index_;

  views::Label* user_name_;
  views::Label* media_capture_label_;
  views::ImageView* media_capture_icon_;

  DISALLOW_COPY_AND_ASSIGN(UserCardView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_
