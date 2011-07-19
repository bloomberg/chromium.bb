// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"

#include "ui/base/l10n/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/content_settings_helper.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/models/table_model_observer.h"

namespace {
// Return -1, 0, or 1 depending on whether |origin1| should be sorted before,
// equal to, or after |origin2|.
int CompareOrigins(const GURL& origin1, const GURL& origin2) {
  if (origin1 == origin2)
    return 0;

  // Sort alphabetically by host name.
  std::string origin1_host(origin1.host());
  std::string origin2_host(origin2.host());
  if (origin1_host != origin2_host)
    return origin1_host < origin2_host ? -1 : 1;

  // We'll show non-HTTP schemes, so sort them alphabetically, but put HTTP
  // first.
  std::string origin1_scheme(origin1.scheme());
  std::string origin2_scheme(origin2.scheme());
  if (origin1_scheme != origin2_scheme) {
    if (origin1_scheme == chrome::kHttpScheme)
      return -1;
    if (origin2_scheme == chrome::kHttpScheme)
      return 1;
    return origin1_scheme < origin2_scheme ? -1 : 1;
  }

  // Sort by port number.  This has to differ if the origins are really origins
  // (and not longer URLs).  An unspecified port will be -1 and thus
  // automatically come first (which is what we want).
  int origin1_port = origin1.IntPort();
  int origin2_port = origin2.IntPort();
  DCHECK(origin1_port != origin2_port);
  return origin1_port < origin2_port ? -1 : 1;
}
}  // namespace

struct GeolocationExceptionsTableModel::Entry {
  Entry(const GURL& in_origin,
        const GURL& in_embedding_origin,
        ContentSetting in_setting)
      : origin(in_origin),
        embedding_origin(in_embedding_origin),
        setting(in_setting) {
  }

  GURL origin;
  GURL embedding_origin;
  ContentSetting setting;
};

GeolocationExceptionsTableModel::GeolocationExceptionsTableModel(
    GeolocationContentSettingsMap* map)
    : map_(map),
      observer_(NULL) {
  GeolocationContentSettingsMap::AllOriginsSettings settings(
      map_->GetAllOriginsSettings());
  GeolocationContentSettingsMap::AllOriginsSettings::const_iterator i;
  for (i = settings.begin(); i != settings.end(); ++i)
    AddEntriesForOrigin(i->first, i->second);
}

GeolocationExceptionsTableModel::~GeolocationExceptionsTableModel() {}

bool GeolocationExceptionsTableModel::CanRemoveRows(
    const Rows& rows) const {
  for (Rows::const_iterator i(rows.begin()); i != rows.end(); ++i) {
    const Entry& entry = entries_[*i];
    if ((entry.origin == entry.embedding_origin) &&
        (entry.setting == CONTENT_SETTING_DEFAULT)) {
      for (size_t j = (*i) + 1;
           (j < entries_.size()) && (entries_[j].origin == entry.origin); ++j) {
        if (!rows.count(j))
          return false;
      }
    }
  }
  return !rows.empty();
}

void GeolocationExceptionsTableModel::RemoveRows(const Rows& rows) {
  for (Rows::const_reverse_iterator i(rows.rbegin()); i != rows.rend(); ++i) {
    size_t row = *i;
    Entry* entry = &entries_[row];
    GURL entry_origin(entry->origin);  // Copy, not reference, since we'll erase
                                       // |entry| before we're done with this.
    bool next_has_same_origin = ((row + 1) < entries_.size()) &&
        (entries_[row + 1].origin == entry_origin);
    bool has_children = (entry_origin == entry->embedding_origin) &&
        next_has_same_origin;
    map_->SetContentSetting(entry_origin, entry->embedding_origin,
                            CONTENT_SETTING_DEFAULT);
    if (has_children) {
      entry->setting = CONTENT_SETTING_DEFAULT;
      if (observer_)
        observer_->OnItemsChanged(row, 1);
      continue;
    }
    do {
      entries_.erase(entries_.begin() + row);  // Note: |entry| is now garbage.
      if (observer_)
        observer_->OnItemsRemoved(row, 1);
      // If we remove the last non-default child of a default parent, we should
      // remove the parent too.  We do these removals one-at-a-time because the
      // table view will end up being called back as each row is removed, in
      // turn calling back to CanRemoveRows(), and if we've already removed
      // more entries than the view has, we'll have problems.
      if ((row == 0) || rows.count(row - 1))
        break;
      entry = &entries_[--row];
    } while (!next_has_same_origin && (entry->origin == entry_origin) &&
             (entry->origin == entry->embedding_origin) &&
             (entry->setting == CONTENT_SETTING_DEFAULT));
  }
}

void GeolocationExceptionsTableModel::RemoveAll() {
  int old_row_count = RowCount();
  entries_.clear();
  map_->ResetToDefault();
  if (observer_)
    observer_->OnItemsRemoved(0, old_row_count);
}

int GeolocationExceptionsTableModel::RowCount() {
  return entries_.size();
}

string16 GeolocationExceptionsTableModel::GetText(int row,
                                                  int column_id) {
  const Entry& entry = entries_[row];
  if (column_id == IDS_EXCEPTIONS_HOSTNAME_HEADER) {
    if (entry.origin == entry.embedding_origin) {
      return content_settings_helper::OriginToString16(entry.origin);
    }
    string16 indent(ASCIIToUTF16("    "));
    if (entry.embedding_origin.is_empty()) {
      // NOTE: As long as the user cannot add/edit entries from the exceptions
      // dialog, it's impossible to actually have a non-default setting for some
      // origin "embedded on any other site", so this row will never appear.  If
      // we add the ability to add/edit exceptions, we'll need to decide when to
      // display this and how "removing" it will function.
      return indent + l10n_util::GetStringUTF16(
          IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ANY_OTHER);
    }
    return indent + l10n_util::GetStringFUTF16(
        IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST,
        content_settings_helper::OriginToString16(entry.embedding_origin));
  }

  if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    switch (entry.setting) {
      case CONTENT_SETTING_ALLOW:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ALLOW_BUTTON);
      case CONTENT_SETTING_BLOCK:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_BLOCK_BUTTON);
      case CONTENT_SETTING_ASK:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ASK_BUTTON);
      case CONTENT_SETTING_DEFAULT:
        return l10n_util::GetStringUTF16(IDS_EXCEPTIONS_NOT_SET_BUTTON);
      default:
        break;
    }
  }

  NOTREACHED();
  return string16();
}

void GeolocationExceptionsTableModel::SetObserver(
    ui::TableModelObserver* observer) {
  observer_ = observer;
}

int GeolocationExceptionsTableModel::CompareValues(int row1,
                                                   int row2,
                                                   int column_id) {
  DCHECK(row1 >= 0 && row1 < RowCount() &&
         row2 >= 0 && row2 < RowCount());

  const Entry& entry1 = entries_[row1];
  const Entry& entry2 = entries_[row2];

  // Sort top-level requesting origins, keeping all embedded (child) rules
  // together.
  int origin_comparison = CompareOrigins(entry1.origin, entry2.origin);
  if (origin_comparison == 0) {
    // The non-embedded rule comes before all embedded rules.
    bool entry1_origins_same = entry1.origin == entry1.embedding_origin;
    bool entry2_origins_same = entry2.origin == entry2.embedding_origin;
    if (entry1_origins_same != entry2_origins_same)
      return entry1_origins_same ? -1 : 1;

    // The "default" embedded rule comes after all other embedded rules.
    bool embedding_origin1_empty = entry1.embedding_origin.is_empty();
    bool embedding_origin2_empty = entry2.embedding_origin.is_empty();
    if (embedding_origin1_empty != embedding_origin2_empty)
      return embedding_origin2_empty ? -1 : 1;

    origin_comparison =
        CompareOrigins(entry1.embedding_origin, entry2.embedding_origin);
  } else if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    // The rows are in different origins.  We need to find out how the top-level
    // origins will compare.
    while (entries_[row1].origin != entries_[row1].embedding_origin)
      --row1;
    while (entries_[row2].origin != entries_[row2].embedding_origin)
      --row2;
  }

  // The entries are at the same "scope".  If we're sorting by action, then do
  // that now.
  if (column_id == IDS_EXCEPTIONS_ACTION_HEADER) {
    int compare_text = l10n_util::CompareString16WithCollator(
        GetCollator(), GetText(row1, column_id), GetText(row2, column_id));
    if (compare_text != 0)
      return compare_text;
  }

  // Sort by the relevant origin.
  return origin_comparison;
}

void GeolocationExceptionsTableModel::AddEntriesForOrigin(
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
