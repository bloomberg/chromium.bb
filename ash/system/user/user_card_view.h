// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_USER_CARD_VIEW_H_
#define ASH_SYSTEM_USER_USER_CARD_VIEW_H_

#include "ash/system/user/login_status.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {

// The view displaying information about the user, such as user's avatar, email
// address, name, and more. View has no borders.
class UserCardView : public views::View {
 public:
  // |max_width| takes effect only if |login_status| is LOGGED_IN_PUBLIC.
  UserCardView(user::LoginStatus login_status,
               int max_width,
               int multiprofile_index);
  virtual ~UserCardView();

 private:
  // Creates the content for the retail logged in mode.
  void AddRetailModeUserContent();

  // Creates the content for the public mode.
  void AddPublicModeUserContent(int max_width);

  void AddUserContent(user::LoginStatus login_status, int multiprofile_index);

  // Create a user icon representation.
  views::View* CreateIcon(user::LoginStatus login_status,
                          int multiprofile_index);

  DISALLOW_COPY_AND_ASSIGN(UserCardView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_USER_CARD_VIEW_H_
