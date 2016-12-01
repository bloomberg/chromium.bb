// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_USER_USER_VIEW_H_
#define ASH_COMMON_SYSTEM_USER_USER_VIEW_H_

#include <memory>

#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/user/tray_user.h"
#include "ash/public/cpp/session_types.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"

namespace gfx {
class Rect;
class Size;
}

namespace views {
class FocusManager;
}

namespace ash {

enum class LoginStatus;
class PopupMessage;
class SystemTrayItem;

namespace tray {

// The view of a user item in system tray bubble.
class UserView : public views::View,
                 public views::ButtonListener,
                 public views::MouseWatcherListener,
                 public views::FocusChangeListener {
 public:
  UserView(SystemTrayItem* owner, LoginStatus login, UserIndex index);
  ~UserView() override;

  // Overridden from MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  TrayUser::TestState GetStateForTest() const;
  gfx::Rect GetBoundsInScreenOfUserButtonForTest();

  views::View* user_card_view_for_test() const { return user_card_view_; }

 private:
  // Retruns true if |this| view is for the currently active user, i.e. top row.
  bool IsActiveUser() const;

  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  void AddLogoutButton(LoginStatus login);
  void AddUserCard(LoginStatus login);
  void AddUserCardMd(LoginStatus login);

  // Create the menu option to add another user. If |disabled| is set the user
  // cannot actively click on the item.
  void ToggleAddUserMenuOption();

  // Removes the add user menu option.
  void RemoveAddUserMenuOption();

  const UserIndex user_index_;
  views::View* user_card_view_;

  // This is the owner system tray item of this view.
  SystemTrayItem* owner_;

  // True if |user_card_view_| is a |ButtonFromView| - otherwise it is only
  // a |UserCardView|.
  bool is_user_card_button_;

  views::View* logout_button_;
  std::unique_ptr<PopupMessage> popup_message_;
  std::unique_ptr<views::Widget> add_menu_option_;

  // False when the add user panel is visible but not activatable.
  bool add_user_enabled_;

  // The mouse watcher which takes care of out of window hover events.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // The focus manager which we use to detect focus changes.
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_USER_USER_VIEW_H_
