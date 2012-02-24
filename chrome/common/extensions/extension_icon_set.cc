// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_icon_set.h"

#include "base/logging.h"

ExtensionIconSet::ExtensionIconSet() {}

ExtensionIconSet::~ExtensionIconSet() {}

const ExtensionIconSet::Icons ExtensionIconSet::kIconSizes[] = {
  EXTENSION_ICON_GIGANTOR,
  EXTENSION_ICON_EXTRA_LARGE,
  EXTENSION_ICON_LARGE,
  EXTENSION_ICON_MEDIUM,
  EXTENSION_ICON_SMALL,
  EXTENSION_ICON_SMALLISH,
  EXTENSION_ICON_BITTY
};

const size_t ExtensionIconSet::kNumIconSizes =
    arraysize(ExtensionIconSet::kIconSizes);

void ExtensionIconSet::Clear() {
  map_.clear();
}

void ExtensionIconSet::Add(Icons size, const std::string& path) {
  DCHECK(!path.empty() && path[0] != '/');
  map_[size] = path;
}

std::string ExtensionIconSet::Get(int size, MatchType match_type) const {
  // The searches for MATCH_BIGGER and MATCH_SMALLER below rely on the fact that
  // std::map is sorted. This is per the spec, so it should be safe to rely on.
  if (match_type == MATCH_EXACTLY) {
    IconMap::const_iterator result = map_.find(static_cast<Icons>(size));
    return result == map_.end() ? std::string() : result->second;
  } else if (match_type == MATCH_SMALLER) {
    IconMap::const_reverse_iterator result = map_.rend();
    for (IconMap::const_reverse_iterator iter = map_.rbegin();
         iter != map_.rend(); ++iter) {
      if (iter->first <= size) {
        result = iter;
        break;
      }
    }
    return result == map_.rend() ? std::string() : result->second;
  } else {
    DCHECK(match_type == MATCH_BIGGER);
    IconMap::const_iterator result = map_.end();
    for (IconMap::const_iterator iter = map_.begin(); iter != map_.end();
         ++iter) {
      if (iter->first >= size) {
        result = iter;
        break;
      }
    }
    return result == map_.end() ? std::string() : result->second;
  }
}

bool ExtensionIconSet::ContainsPath(const std::string& path) const {
  return GetIconSizeFromPath(path) != EXTENSION_ICON_INVALID;
}

ExtensionIconSet::Icons ExtensionIconSet::GetIconSizeFromPath(
    const std::string& path) const {
  if (path.empty())
    return EXTENSION_ICON_INVALID;

  DCHECK(path[0] != '/') <<
      "ExtensionIconSet stores icon paths without leading slash.";

  for (IconMap::const_iterator iter = map_.begin(); iter != map_.end();
       ++iter) {
    if (iter->second == path)
      return iter->first;
  }

  return EXTENSION_ICON_INVALID;
}
