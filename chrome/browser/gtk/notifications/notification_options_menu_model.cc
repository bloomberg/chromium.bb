// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/notifications/notification_options_menu_model.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

using menus::MenuModel;

NotificationOptionsMenuModel::NotificationOptionsMenuModel(Balloon* balloon)
    : balloon_(balloon) {
}

NotificationOptionsMenuModel::~NotificationOptionsMenuModel() {
}

bool NotificationOptionsMenuModel::HasIcons() const {
  return false;
}

int NotificationOptionsMenuModel::GetItemCount() const {
  return 1;
}

MenuModel::ItemType NotificationOptionsMenuModel::GetTypeAt(int index) const {
  return MenuModel::TYPE_COMMAND;
}

int NotificationOptionsMenuModel::GetCommandIdAt(int index) const {
  return index;
}

string16 NotificationOptionsMenuModel::GetLabelAt(int index) const {
  DCHECK_EQ(0, index);

  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
      WideToUTF16(balloon_->notification().display_source()));
}

bool NotificationOptionsMenuModel::IsLabelDynamicAt(int index) const {
  return false;
}

bool NotificationOptionsMenuModel::GetAcceleratorAt(
    int index, menus::Accelerator* accelerator) const {
  return false;
}

bool NotificationOptionsMenuModel::IsItemCheckedAt(int index) const {
  return false;
}

int NotificationOptionsMenuModel::GetGroupIdAt(int index) const {
  return 0;
}

bool NotificationOptionsMenuModel::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

bool NotificationOptionsMenuModel::IsEnabledAt(int index) const {
  return true;
}

MenuModel* NotificationOptionsMenuModel::GetSubmenuModelAt(int index) const {
  return NULL;
}

void NotificationOptionsMenuModel::HighlightChangedTo(int index) {
}

void NotificationOptionsMenuModel::ActivatedAt(int index) {
  DCHECK_EQ(0, index);

  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();

  service->DenyPermission(balloon_->notification().origin_url());
}
