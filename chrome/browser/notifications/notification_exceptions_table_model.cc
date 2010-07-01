// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "app/table_model_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

NotificationExceptionsTableModel::NotificationExceptionsTableModel(
    DesktopNotificationService* service)
    : service_(service),
      observer_(NULL) {
}

bool NotificationExceptionsTableModel::CanRemoveRows(
    const Rows& rows) const {
  NOTIMPLEMENTED();
  return false;
}

void NotificationExceptionsTableModel::RemoveRows(const Rows& rows) {
  NOTIMPLEMENTED();
}

void NotificationExceptionsTableModel::RemoveAll() {
  NOTIMPLEMENTED();
}

int NotificationExceptionsTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring NotificationExceptionsTableModel::GetText(int row,
                                                       int column_id) {
  NOTIMPLEMENTED();
  return std::wstring();
}

void NotificationExceptionsTableModel::SetObserver(
    TableModelObserver* observer) {
  observer_ = observer;
}

NotificationExceptionsTableModel::Entry::Entry(
    const GURL& in_origin,
    ContentSetting in_setting)
    : origin(in_origin),
      setting(in_setting) {
}

