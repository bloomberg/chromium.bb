// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_exceptions_table_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

ContentExceptionsTableModel::ContentExceptionsTableModel(
    HostContentSettingsMap* map,
    HostContentSettingsMap* off_the_record_map,
    ContentSettingsType type)
    : map_(map),
      off_the_record_map_(off_the_record_map),
      content_type_(type),
      observer_(NULL) {
  // Load the contents.
  map->GetSettingsForOneType(type, "", &entries_);
  if (off_the_record_map) {
    off_the_record_map->GetSettingsForOneType(type,
                                              "",
                                              &off_the_record_entries_);
  }
}

ContentExceptionsTableModel::~ContentExceptionsTableModel() {}

void ContentExceptionsTableModel::AddException(
    const ContentSettingsPattern& original_pattern,
    ContentSetting setting,
    bool is_off_the_record) {
  DCHECK(!is_off_the_record || off_the_record_map_);

  int insert_position =
      is_off_the_record ? RowCount() : static_cast<int>(entries_.size());

  const ContentSettingsPattern pattern(
      original_pattern.CanonicalizePattern());

  entries(is_off_the_record).push_back(
      HostContentSettingsMap::PatternSettingPair(pattern, setting));
  map(is_off_the_record)->SetContentSetting(pattern,
                                            content_type_,
                                            "",
                                            setting);
  if (observer_)
    observer_->OnItemsAdded(insert_position, 1);
}

void ContentExceptionsTableModel::RemoveException(int row) {
  bool is_off_the_record = entry_is_off_the_record(row);
  int position_to_delete =
      is_off_the_record ? row - static_cast<int>(entries_.size()) : row;
  const HostContentSettingsMap::PatternSettingPair& pair = entry_at(row);

  map(is_off_the_record)->SetContentSetting(
      pair.first, content_type_, "", CONTENT_SETTING_DEFAULT);
  entries(is_off_the_record).erase(
      entries(is_off_the_record).begin() + position_to_delete);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void ContentExceptionsTableModel::RemoveAll() {
  int old_row_count = RowCount();
  entries_.clear();
  off_the_record_entries_.clear();
  map_->ClearSettingsForOneType(content_type_);
  if (off_the_record_map_)
    off_the_record_map_->ClearSettingsForOneType(content_type_);
  if (observer_)
    observer_->OnItemsRemoved(0, old_row_count);
}

int ContentExceptionsTableModel::IndexOfExceptionByPattern(
    const ContentSettingsPattern& original_pattern,
    bool is_off_the_record) {
  DCHECK(!is_off_the_record || off_the_record_map_);

  int offset =
      is_off_the_record ? static_cast<int>(entries_.size()) : 0;

  const ContentSettingsPattern pattern(
      original_pattern.CanonicalizePattern());

  // This is called on every key type in the editor. Move to a map if we end up
  // with lots of exceptions.
  for (size_t i = 0; i < entries(is_off_the_record).size(); ++i) {
    if (entries(is_off_the_record)[i].first == pattern)
      return offset + static_cast<int>(i);
  }
  return -1;
}

int ContentExceptionsTableModel::RowCount() {
  return static_cast<int>(entries_.size() + off_the_record_entries_.size());
}

string16 ContentExceptionsTableModel::GetText(int row, int column_id) {
  HostContentSettingsMap::PatternSettingPair entry = entry_at(row);

  switch (column_id) {
    case IDS_EXCEPTIONS_PATTERN_HEADER:
      return UTF8ToUTF16(entry.first.AsString());

    case IDS_EXCEPTIONS_ACTION_HEADER:
      switch (entry.second) {
        case CONTENT_SETTING_ALLOW:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON);
        case CONTENT_SETTING_BLOCK:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON);
        case CONTENT_SETTING_ASK:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ASK_BUTTON);
        case CONTENT_SETTING_SESSION_ONLY:
          return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_SESSION_ONLY_BUTTON);
        default:
          NOTREACHED();
      }
      break;

    default:
      NOTREACHED();
  }

  return string16();
}

void ContentExceptionsTableModel::SetObserver(
    ui::TableModelObserver* observer) {
  observer_ = observer;
}
