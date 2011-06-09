// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"

#include "base/logging.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"

namespace content_settings {

OriginIdentifierValueMap::OriginIdentifierValueMap() {}

OriginIdentifierValueMap::~OriginIdentifierValueMap() {
  Clear();
}

bool operator>(const OriginIdentifierValueMap::Entry& first,
               const OriginIdentifierValueMap::Entry& second) {
  // Compare item patterns.
  if (first.item_pattern > second.item_pattern)
    return true;
  if (first.item_pattern < second.item_pattern)
    return false;

  // Compare top_level_frame patterns.
  if (first.top_level_frame_pattern > second.top_level_frame_pattern)
    return true;
  return false;
}

Value* OriginIdentifierValueMap::GetValue(
    const GURL& item_url,
    const GURL& top_level_frame_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // Find best matching list entry.
  OriginIdentifierValueMap::const_iterator best_match = entries_.end();
  for (OriginIdentifierValueMap::const_iterator entry = entries_.begin();
       entry != entries_.end();
       ++entry) {
    if (entry->item_pattern.Matches(item_url) &&
        entry->top_level_frame_pattern.Matches(top_level_frame_url) &&
        entry->content_type == content_type &&
        entry->identifier == resource_identifier) {
      if (best_match == entries_.end() || *entry > *best_match) {
        best_match = entry;
      }
    }
  }
  if (best_match != entries_.end())
    return best_match->value.get();
  return NULL;
}

void OriginIdentifierValueMap::SetValue(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* value) {
  OriginIdentifierValueMap::iterator list_entry =
      FindEntry(item_pattern,
                top_level_frame_pattern,
                content_type,
                resource_identifier);
  if (list_entry == entries_.end()) {
    // No matching list entry found. Add a new entry to the list.
    entries_.insert(list_entry, Entry(item_pattern,
                                      top_level_frame_pattern,
                                      content_type,
                                      resource_identifier,
                                      value));
  } else {
    // Update the list entry.
    list_entry->value.reset(value);
  }
}

void OriginIdentifierValueMap::DeleteValue(
      const ContentSettingsPattern& item_pattern,
      const ContentSettingsPattern& top_level_frame_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) {
  OriginIdentifierValueMap::iterator entry_to_delete =
      FindEntry(item_pattern,
                top_level_frame_pattern,
                content_type,
                resource_identifier);
  if (entry_to_delete != entries_.end()) {
    entries_.erase(entry_to_delete);
  }
}

void OriginIdentifierValueMap::Clear() {
  // Delete all owned value objects.
  /*
  for (OriginIdentifierValueMap::iterator entry = entries_.begin();
       entry != entries_.end();
       ++entry) {
    delete entry->value;
  }
  */
  entries_.clear();
}

OriginIdentifierValueMap::iterator OriginIdentifierValueMap::DeleteValue(
    OriginIdentifierValueMap::iterator entry) {
  return entries_.erase(entry);
}

OriginIdentifierValueMap::iterator OriginIdentifierValueMap::FindEntry(
      const ContentSettingsPattern& item_pattern,
      const ContentSettingsPattern& top_level_frame_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) {
  for (OriginIdentifierValueMap::iterator entry = entries_.begin();
       entry != entries_.end();
       ++entry) {
    if (item_pattern == entry->item_pattern &&
        top_level_frame_pattern == entry->top_level_frame_pattern &&
        content_type == entry->content_type &&
        resource_identifier == entry->identifier) {
      return entry;
    }
  }
  return entries_.end();
}

}  // namespace content_settings
