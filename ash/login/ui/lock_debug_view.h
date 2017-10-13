// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
#define ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_

#include <memory>
#include <vector>

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
}

namespace ash {

class LoginDataDispatcher;
class LockContentsView;

namespace mojom {
enum class TrayActionState;
}

// Contains the debug UI row (ie, add user, toggle PIN buttons).
class LockDebugView : public views::View, public views::ButtonListener {
 public:
  LockDebugView(mojom::TrayActionState initial_note_action_state,
                LoginDataDispatcher* data_dispatcher);
  ~LockDebugView() override;

  // views::View:
  void Layout() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  class DebugDataDispatcherTransformer;

  // Rebuilds the debug user column which contains per-user actions.
  void RebuildDebugUserColumn();

  // Creates a button on the debug row that cannot be focused.
  views::MdTextButton* AddButton(const std::string& text,
                                 bool add_to_debug_row = true);

  LockContentsView* lock_ = nullptr;

  // User column which contains per-user actions.
  views::View* per_user_action_column_ = nullptr;
  std::vector<views::View*> per_user_action_column_toggle_pin_;

  // Debug row which contains buttons that affect the entire UI.
  views::View* debug_row_ = nullptr;
  views::MdTextButton* toggle_blur_ = nullptr;
  views::MdTextButton* toggle_note_action_ = nullptr;
  views::MdTextButton* toggle_caps_lock_ = nullptr;
  views::MdTextButton* add_user_ = nullptr;
  views::MdTextButton* remove_user_ = nullptr;
  views::MdTextButton* toggle_auth_ = nullptr;

  // Debug dispatcher and cached data for the UI.
  std::unique_ptr<DebugDataDispatcherTransformer> const debug_data_dispatcher_;
  size_t num_users_ = 1u;
  bool force_fail_auth_ = false;

  DISALLOW_COPY_AND_ASSIGN(LockDebugView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
