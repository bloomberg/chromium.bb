// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_exceptions_table_model.h"

#include "base/auto_reset.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_helper.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

struct NotificationExceptionsTableModel::Entry {
  Entry(const GURL& origin, ContentSetting setting);
  bool operator<(const Entry& b) const;

  GURL origin;
  ContentSetting setting;
};

NotificationExceptionsTableModel::NotificationExceptionsTableModel(
    DesktopNotificationService* service)
    : service_(service),
      updates_disabled_(false),
      observer_(NULL) {
  registrar_.Add(this, NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED,
                 NotificationService::AllSources());
  LoadEntries();
}

NotificationExceptionsTableModel::~NotificationExceptionsTableModel() {}

bool NotificationExceptionsTableModel::CanRemoveRows(
    const Rows& rows) const {
  return !rows.empty();
}

void NotificationExceptionsTableModel::RemoveRows(const Rows& rows) {
  AutoReset<bool> tmp(&updates_disabled_, true);
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
  AutoReset<bool> tmp(&updates_disabled_, true);
  entries_.clear();
  service_->ResetAllOrigins();
  if (observer_)
    observer_->OnModelChanged();
}

int NotificationExceptionsTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

string16 NotificationExceptionsTableModel::GetText(int row,
                                                   int column_id) {
  const Entry& entry = entries_[row];
  if (column_id == IDS_EXCEPTIONS_HOSTNAME_HEADER) {
    return content_settings_helper::OriginToString16(entry.origin);
  }

  if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    switch (entry.setting) {
      case CONTENT_SETTING_ALLOW:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON);
      case CONTENT_SETTING_BLOCK:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON);
      default:
        break;
    }
  }

  NOTREACHED();
  return string16();
}

void NotificationExceptionsTableModel::SetObserver(
    ui::TableModelObserver* observer) {
  observer_ = observer;
}

void NotificationExceptionsTableModel::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (!updates_disabled_) {
    DCHECK(type == NotificationType::DESKTOP_NOTIFICATION_SETTINGS_CHANGED);
    entries_.clear();
    LoadEntries();

    if (observer_)
      observer_->OnModelChanged();
  }
}

void NotificationExceptionsTableModel::LoadEntries() {
  std::vector<GURL> allowed(service_->GetAllowedOrigins());
  std::vector<GURL> blocked(service_->GetBlockedOrigins());
  entries_.reserve(allowed.size() + blocked.size());
  for (size_t i = 0; i < allowed.size(); ++i)
    entries_.push_back(Entry(allowed[i], CONTENT_SETTING_ALLOW));
  for (size_t i = 0; i < blocked.size(); ++i)
    entries_.push_back(Entry(blocked[i], CONTENT_SETTING_BLOCK));
  std::sort(entries_.begin(), entries_.end());
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
