// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_content_settings_table_model.h"

#include "app/l10n_util.h"
#include "app/table_model_observer.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"

GeolocationContentSettingsTableModel::GeolocationContentSettingsTableModel(
    GeolocationContentSettingsMap* map)
    : map_(map),
      observer_(NULL) {
  GeolocationContentSettingsMap::AllOriginsSettings settings(
      map_->GetAllOriginsSettings());
  GeolocationContentSettingsMap::AllOriginsSettings::const_iterator i;
  for (i = settings.begin(); i != settings.end(); ++i)
    AddEntriesForOrigin(i->first, i->second);
}

bool GeolocationContentSettingsTableModel::CanRemoveException(int row) const {
  const Entry& entry = entries_[row];
  return !(entry.origin == entry.embedding_origin &&
           static_cast<size_t>(row + 1) < entries_.size() &&
           entries_[row + 1].origin == entry.origin &&
           entry.setting == CONTENT_SETTING_DEFAULT);
}

void GeolocationContentSettingsTableModel::RemoveException(int row) {
  Entry& entry = entries_[row];
  bool next_has_same_origin = static_cast<size_t>(row + 1) < entries_.size() &&
      entries_[row + 1].origin == entry.origin;
  bool has_children = entry.origin == entry.embedding_origin &&
      next_has_same_origin;
  map_->SetContentSetting(entry.origin, entry.embedding_origin,
                          CONTENT_SETTING_DEFAULT);
  if (has_children) {
    entry.setting = CONTENT_SETTING_DEFAULT;
    if (observer_)
      observer_->OnItemsChanged(row, 1);
  } else if (!next_has_same_origin &&
        row > 0 &&
        entries_[row - 1].origin == entry.origin &&
        entries_[row - 1].setting == CONTENT_SETTING_DEFAULT) {
    // If we remove the last non-default child of a default parent, we should
    // remove the parent too.
    entries_.erase(entries_.begin() + row - 1, entries_.begin() + row + 1);
    if (observer_)
      observer_->OnItemsRemoved(row - 1, 2);
  } else {
    entries_.erase(entries_.begin() + row);
    if (observer_)
      observer_->OnItemsRemoved(row, 1);
  }
}

void GeolocationContentSettingsTableModel::RemoveAll() {
  int old_row_count = RowCount();
  entries_.clear();
  map_->ResetToDefault();
  if (observer_)
    observer_->OnItemsRemoved(0, old_row_count);
}

int GeolocationContentSettingsTableModel::RowCount() {
  return entries_.size();
}

std::wstring GeolocationContentSettingsTableModel::GetText(int row,
                                                           int column_id) {
  const Entry& entry = entries_[row];
  if (column_id == IDS_EXCEPTIONS_HOSTNAME_HEADER) {
    if (entry.origin == entry.embedding_origin)
      return UTF8ToWide(
          GeolocationContentSettingsMap::OriginToString(entry.origin));
    if (entry.embedding_origin.is_empty())
      return ASCIIToWide("  ") +
          l10n_util::GetString(IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ANY_OTHER);
    return ASCIIToWide("  ") +
        l10n_util::GetStringF(IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST,
            UTF8ToWide(GeolocationContentSettingsMap::OriginToString(
                entry.embedding_origin)));
  } else if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    switch (entry.setting) {
      case CONTENT_SETTING_ALLOW:
        return l10n_util::GetString(IDS_EXCEPTIONS_ALLOW_BUTTON);
      case CONTENT_SETTING_BLOCK:
        return l10n_util::GetString(IDS_EXCEPTIONS_BLOCK_BUTTON);
      case CONTENT_SETTING_ASK:
        return l10n_util::GetString(IDS_EXCEPTIONS_ASK_BUTTON);
      case CONTENT_SETTING_DEFAULT:
        return l10n_util::GetString(IDS_EXCEPTIONS_NOT_SET_BUTTON);
      default:
        NOTREACHED();
    }
  } else {
    NOTREACHED();
  }
  return std::wstring();
}

void GeolocationContentSettingsTableModel::SetObserver(
    TableModelObserver* observer) {
  observer_ = observer;
}

void GeolocationContentSettingsTableModel::AddEntriesForOrigin(
    const GURL& origin,
    const GeolocationContentSettingsMap::OneOriginSettings& settings) {
  GeolocationContentSettingsMap::OneOriginSettings::const_iterator parent =
      settings.find(origin);

  // Add the "parent" entry for the non-embedded setting.
  entries_.push_back(Entry(origin, origin,
      (parent == settings.end()) ? CONTENT_SETTING_DEFAULT : parent->second));

  // Add the "children" for any embedded settings.
  GeolocationContentSettingsMap::OneOriginSettings::const_iterator i;
  for (i = settings.begin(); i != settings.end(); ++i) {
    // Skip the non-embedded setting which we already added above.
    if (i == parent)
      continue;

    entries_.push_back(Entry(origin, i->first, i->second));
  }
}

// static
GeolocationContentSettingsTableModel::Entry::Entry(
    const GURL& in_origin, const GURL& in_embedding_origin,
    ContentSetting in_setting)
    : origin(in_origin),
      embedding_origin(in_embedding_origin),
      setting(in_setting) {
}
