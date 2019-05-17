// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_widget.h"

#include <utility>

#include "ash/login/ui/parent_access_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

ParentAccessWidget::ParentAccessWidget(
    const base::Optional<AccountId>& account_id,
    const OnExitCallback& callback) {
  views::Widget::InitParams widget_params;
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? ash::kShellWindowId_SystemModalContainer
          : ash::kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      ash::Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);

  widget_ = std::make_unique<views::Widget>();
  widget_->set_focus_on_creation(true);
  widget_->Init(widget_params);

  ParentAccessView::Callbacks callbacks;
  callbacks.on_finished = callback;

  widget_->SetContentsView(new ParentAccessView(account_id, callbacks));
  widget_->CenterWindow(widget_->GetContentsView()->GetPreferredSize());
  widget_->Show();
  widget_->GetContentsView()->RequestFocus();
}

ParentAccessWidget::~ParentAccessWidget() = default;

}  // namespace ash
