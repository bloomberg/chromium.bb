// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/tray_supervised_user.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/supervised/supervised_notification_controller.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/callback.h"
#include "base/logging.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

const gfx::VectorIcon& GetSupervisedUserIcon() {
  SessionController* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller->IsUserSupervised());

  if (session_controller->IsUserChild())
    return kSystemMenuChildUserIcon;

  return kSystemMenuSupervisedUserIcon;
}

}  // namespace

TraySupervisedUser::TraySupervisedUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_SUPERVISED_USER) {}

TraySupervisedUser::~TraySupervisedUser() = default;

views::View* TraySupervisedUser::CreateDefaultView(LoginStatus status) {
  if (!Shell::Get()->session_controller()->IsUserSupervised())
    return nullptr;

  LabelTrayView* tray_view =
      new LabelTrayView(nullptr, GetSupervisedUserIcon());
  // The message almost never changes during a session, so we compute it when
  // the menu is shown. We don't update it while the menu is open.
  tray_view->SetMessage(
      SupervisedNotificationController::GetSupervisedUserMessage());
  return tray_view;
}

}  // namespace ash
