// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_

#include <list>
#include <map>
#include <string>

#include "base/tuple.h"
#include "base/memory/linked_ptr.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/content_settings_types.h"

class GURL;

namespace base {
class Value;
}

namespace content_settings {

class OriginIdentifierValueMap {
 public:
  typedef std::string ResourceIdentifier;

  struct Entry {
    Entry(const ContentSettingsPattern& primary_pattern,
          const ContentSettingsPattern& secondary_pattern,
          ContentSettingsType content_type,
          ResourceIdentifier identifier,
          base::Value* value);
    ~Entry();

    ContentSettingsPattern primary_pattern;
    ContentSettingsPattern secondary_pattern;
    ContentSettingsType content_type;
    ResourceIdentifier identifier;
    linked_ptr<base::Value> value;
  };

  typedef std::list<Entry> EntryList;

  typedef EntryList::const_iterator const_iterator;

  typedef EntryList::iterator iterator;

  EntryList::iterator begin() {
    return entries_.begin();
  }

  EntryList::iterator end() {
    return entries_.end();
  }

  EntryList::const_iterator begin() const {
    return entries_.begin();
  }

  EntryList::const_iterator end() const {
    return entries_.end();
  }

  bool empty() const {
    return size() == 0u;
  }

  size_t size() const {
    return entries_.size();
  }

  OriginIdentifierValueMap();
  ~OriginIdentifierValueMap();

  // Returns a weak pointer to the value for the given |primary_pattern|,
  // |secondary_pattern|, |content_type|, |resource_identifier| tuple. If
  // no value is stored for the passed parameter |NULL| is returned.
  base::Value* GetValue(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) const;

  // Sets the |value| for the given |primary_pattern|, |secondary_pattern|,
  // |content_type|, |resource_identifier| tuple. The method takes the ownership
  // of the passed |value|.
  void SetValue(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      base::Value* value);

  // Deletes the map entry for the given |primary_pattern|,
  // |secondary_pattern|, |content_type|, |resource_identifier| tuple.
  void DeleteValue(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier);

  // Deletes the map entry at the passed position. The method returns the
  // position of the next entry in the map.
  EntryList::iterator erase(EntryList::iterator entry);

  // Clears all map entries.
  void clear();

 private:
  // Finds the list entry for the given |primary_pattern|,
  // |secondary_pattern|, |content_type|, |resource_identifier| tuple and
  // returns the iterator of the list entry. If no entry is found for the passed
  // parameters then the end of list iterator is returned.
  EntryList::iterator FindEntry(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier);

  EntryList entries_;

  DISALLOW_COPY_AND_ASSIGN(OriginIdentifierValueMap);
};

// Compares two origin value map entries and tests if the item, top-level-frame
// pattern pair of the first entry has a higher precedences then the pattern
// pair of the second entry.
bool operator>(const OriginIdentifierValueMap::Entry& first,
               const OriginIdentifierValueMap::Entry& second);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
