// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_USER_VIEW_H_
#define ASH_SYSTEM_USER_USER_VIEW_H_

#include "ash/session/session_state_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/tray_user.h"
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

class PopupMessage;
class SystemTrayItem;

namespace tray {

// The view of a user item in system tray bubble.
class UserView : public views::View,
                 public views::ButtonListener,
                 public views::MouseWatcherListener,
                 public views::FocusChangeListener {
 public:
  UserView(SystemTrayItem* owner,
           ash::user::LoginStatus login,
           MultiProfileIndex index,
           bool for_detailed_view);
  virtual ~UserView();

  // Overridden from MouseWatcherListener:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  TrayUser::TestState GetStateForTest() const;
  gfx::Rect GetBoundsInScreenOfUserButtonForTest();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::FocusChangeListener:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  void AddLogoutButton(user::LoginStatus login);
  void AddUserCard(user::LoginStatus login);

  // Create the menu option to add another user. If |disabled| is set the user
  // cannot actively click on the item.
  void ToggleAddUserMenuOption();

  // Removes the add user menu option.
  void RemoveAddUserMenuOption();

  MultiProfileIndex multiprofile_index_;
  // The view of the user card.
  views::View* user_card_view_;

  // This is the owner system tray item of this view.
  SystemTrayItem* owner_;

  // True if |user_card_view_| is a |ButtonFromView| - otherwise it is only
  // a |UserCardView|.
  bool is_user_card_button_;

  views::View* logout_button_;
  scoped_ptr<PopupMessage> popup_message_;
  scoped_ptr<views::Widget> add_menu_option_;

  // False when the add user panel is visible but not activatable.
  bool add_user_enabled_;

  // True if this view will be used inside detailed view.
  bool for_detailed_view_;

  // The mouse watcher which takes care of out of window hover events.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  // The focus manager which we use to detect focus changes.
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_USER_VIEW_H_
