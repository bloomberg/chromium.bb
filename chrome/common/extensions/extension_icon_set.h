// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ICON_SET_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ICON_SET_H_
#pragma once

#include <map>
#include <string>

// Represents the set of icons for an extension.
class ExtensionIconSet {
 public:
  // NOTE: If you change this list, you should also change kIconSizes in the cc
  // file.
  enum Icons {
    EXTENSION_ICON_GIGANTOR = 512,
    EXTENSION_ICON_EXTRA_LARGE = 256,
    EXTENSION_ICON_LARGE = 128,
    EXTENSION_ICON_MEDIUM = 48,
    EXTENSION_ICON_SMALL = 32,
    EXTENSION_ICON_SMALLISH = 24,
    EXTENSION_ICON_BITTY = 16,
    EXTENSION_ICON_INVALID = 0,
  };

  // Get an icon from the set, optionally falling back to a smaller or bigger
  // size. MatchType is exclusive (do not OR them together).
  enum MatchType {
    MATCH_EXACTLY,
    MATCH_BIGGER,
    MATCH_SMALLER
  };

  // Access to the underlying map from icon size->path.
  typedef std::map<Icons, std::string> IconMap;

  // Icon sizes used by the extension system.
  static const Icons kIconSizes[];
  static const size_t kNumIconSizes;

  ExtensionIconSet();
  ~ExtensionIconSet();

  const IconMap& map() const { return map_; }

  // Remove all icons from the set.
  void Clear();

  // Add an icon to the set. If the specified size is already present, it is
  // overwritten.
  void Add(Icons size, const std::string& path);

  std::string Get(int size, MatchType match_type) const;

  // Returns true if the set contains the specified path.
  bool ContainsPath(const std::string& path) const;

  // Returns icon size if the set contains the specified path or 0 if not found.
  Icons GetIconSizeFromPath(const std::string& path) const;

 private:
  IconMap map_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ICON_SET_H_
