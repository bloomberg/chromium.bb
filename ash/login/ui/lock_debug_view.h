// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
#define ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_

#include <memory>
#include <vector>

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class LoginDataDispatcher;
class LockContentsView;

namespace mojom {
enum class TrayActionState;
}

// Contains the debug UI strip (ie, add user, toggle PIN buttons).
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

  std::unique_ptr<DebugDataDispatcherTransformer> const debug_data_dispatcher_;
  size_t num_users_ = 1u;

  LockContentsView* lock_;

  views::View* debug_;
  views::View* toggle_blur_;
  views::View* toggle_note_action_;
  views::View* add_user_;
  views::View* remove_user_;

  views::View* user_column_;
  std::vector<views::View*> user_column_entries_toggle_pin_;

  DISALLOW_COPY_AND_ASSIGN(LockDebugView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
