// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  ExtensionIconSet();
  ~ExtensionIconSet();

  // Access to the underlying map from icon size->path.
  typedef std::map<int, std::string> IconMap;
  const IconMap& map() const { return map_; }

  // Remove all icons from the set.
  void Clear();

  // Add an icon to the set. If the specified size is already present, it is
  // overwritten.
  void Add(int size, const std::string& path);

  // Get an icon from the set, optionally falling back to a smaller or bigger
  // size. MatchType is exclusive (do not OR them together).
  enum MatchType {
    MATCH_EXACTLY,
    MATCH_BIGGER,
    MATCH_SMALLER
  };
  std::string Get(int size, MatchType match_type) const;

  // Returns true if the set contains the specified path.
  bool ContainsPath(const std::string& path) const;

 private:
  IconMap map_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ICON_SET_H_
