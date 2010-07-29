// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "app/l10n_util.h"
#include "app/table_model_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/content_settings_helper.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

NotificationExceptionsTableModel::NotificationExceptionsTableModel(
    DesktopNotificationService* service)
    : service_(service),
      observer_(NULL) {
  std::vector<GURL> allowed(service_->GetAllowedOrigins());
  std::vector<GURL> blocked(service_->GetBlockedOrigins());
  entries_.reserve(allowed.size() + blocked.size());
  for (size_t i = 0; i < allowed.size(); ++i)
    entries_.push_back(Entry(allowed[i], CONTENT_SETTING_ALLOW));
  for (size_t i = 0; i < blocked.size(); ++i)
    entries_.push_back(Entry(blocked[i], CONTENT_SETTING_BLOCK));
  sort(entries_.begin(), entries_.end());
}

bool NotificationExceptionsTableModel::CanRemoveRows(
    const Rows& rows) const {
  return !rows.empty();
}

void NotificationExceptionsTableModel::RemoveRows(const Rows& rows) {
  // This is O(n^2) in rows.size(). Since n is small, that's ok.
  for (Rows::const_reverse_iterator i(rows.rbegin()); i != rows.rend(); ++i) {
    size_t row = *i;
    Entry* entry = &entries_[row];
    if (entry->setting == CONTENT_SETTING_ALLOW) {
      service_->ResetAllowedOrigin(entry->origin);
    } else {
      DCHECK_EQ(entry->setting, CONTENT_SETTING_BLOCK);
      service_->ResetBlockedOrigin(entry->origin);
    }
    entries_.erase(entries_.begin() + row);  // Note: |entry| is now garbage.
    if (observer_)
      observer_->OnItemsRemoved(row, 1);
  }
}

void NotificationExceptionsTableModel::RemoveAll() {
  int old_row_count = RowCount();
  entries_.clear();
  service_->ResetAllOrigins();
  if (observer_)
    observer_->OnItemsRemoved(0, old_row_count);
}

int NotificationExceptionsTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring NotificationExceptionsTableModel::GetText(int row,
                                                       int column_id) {
  const Entry& entry = entries_[row];
  if (column_id == IDS_EXCEPTIONS_HOSTNAME_HEADER) {
    return content_settings_helper::OriginToWString(entry.origin);
  }

  if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    switch (entry.setting) {
      case CONTENT_SETTING_ALLOW:
        return l10n_util::GetString(IDS_EXCEPTIONS_ALLOW_BUTTON);
      case CONTENT_SETTING_BLOCK:
        return l10n_util::GetString(IDS_EXCEPTIONS_BLOCK_BUTTON);
      default:
        break;
    }
  }

  NOTREACHED();
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

bool NotificationExceptionsTableModel::Entry::operator<(
    const NotificationExceptionsTableModel::Entry& b) const {
  DCHECK_NE(origin, b.origin);
  return origin < b.origin;
}
