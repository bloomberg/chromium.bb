// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_
#define ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

enum class LoginStatus;

namespace tray {

// The view displaying information about the user, such as user's avatar, email
// address, name, and more. View has no borders.
class UserCardView : public views::View {
 public:
  // |max_width| takes effect only if |login_status| is LOGGED_IN_PUBLIC.
  UserCardView(LoginStatus login_status, int max_width, int user_index);
  ~UserCardView() override;

  // views::View overrides.
  void PaintChildren(const ui::PaintContext& context) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // Creates the content for the public mode.
  void AddPublicModeUserContent(int max_width);

  void AddUserContent(LoginStatus login_status);
  void AddUserContentMd(LoginStatus login_status);

  bool is_active_user() const { return !user_index_; }

  int user_index_;

  DISALLOW_COPY_AND_ASSIGN(UserCardView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_USER_USER_CARD_VIEW_H_
