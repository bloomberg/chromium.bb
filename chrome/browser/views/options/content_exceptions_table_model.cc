// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_exceptions_table_model.h"

#include "app/l10n_util.h"
#include "app/table_model_observer.h"
#include "chrome/browser/host_content_settings_map.h"
#include "grit/generated_resources.h"

ContentExceptionsTableModel::ContentExceptionsTableModel(
    HostContentSettingsMap* map,
    ContentSettingsType type)
    : map_(map),
      content_type_(type),
      observer_(NULL) {
  // Load the contents.
  map->GetSettingsForOneType(type, &entries_);
}

void ContentExceptionsTableModel::AddException(const std::string& host,
                                        ContentSetting setting) {
  entries_.push_back(HostContentSettingsMap::HostSettingPair(host, setting));
  map_->SetContentSetting(host, content_type_, setting);
  if (observer_)
    observer_->OnItemsAdded(RowCount() - 1, 1);
}

void ContentExceptionsTableModel::RemoveException(int row) {
  const HostContentSettingsMap::HostSettingPair& pair = entries_[row];
  map_->SetContentSetting(pair.first, content_type_, CONTENT_SETTING_DEFAULT);
  entries_.erase(entries_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void ContentExceptionsTableModel::RemoveAll() {
  int old_row_count = RowCount();
  entries_.clear();
  map_->ClearSettingsForOneType(content_type_);
  observer_->OnItemsRemoved(0, old_row_count);
}

int ContentExceptionsTableModel::IndexOfExceptionByHost(
    const std::string& host) {
  // This is called on every key type in the editor. Move to a map if we end up
  // with lots of exceptions.
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].first == host)
      return static_cast<int>(i);
  }
  return -1;
}

int ContentExceptionsTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring ContentExceptionsTableModel::GetText(int row, int column_id) {
  HostContentSettingsMap::HostSettingPair entry = entries_[row];

  switch (column_id) {
    case IDS_EXCEPTIONS_HOSTNAME_HEADER:
      return UTF8ToWide(entry.first);

    case IDS_EXCEPTIONS_ACTION_HEADER:
      switch (entry.second) {
        case CONTENT_SETTING_ALLOW:
          return l10n_util::GetString(IDS_EXCEPTIONS_ALLOW_BUTTON);
        case CONTENT_SETTING_BLOCK:
          return l10n_util::GetString(IDS_EXCEPTIONS_BLOCK_BUTTON);
        case CONTENT_SETTING_ASK:
          return l10n_util::GetString(IDS_EXCEPTIONS_ASK_BUTTON);
        default:
          NOTREACHED();
      }
      break;

    default:
      NOTREACHED();
  }

  return std::wstring();
}

void ContentExceptionsTableModel::SetObserver(TableModelObserver* observer) {
  observer_ = observer;
}
