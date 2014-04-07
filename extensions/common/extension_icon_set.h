// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_ICON_SET_H_
#define EXTENSIONS_COMMON_EXTENSION_ICON_SET_H_

#include <map>
#include <string>

// Represents the set of icons for an extension.
class ExtensionIconSet {
 public:
  // Get an icon from the set, optionally falling back to a smaller or bigger
  // size. MatchType is exclusive (do not OR them together).
  enum MatchType {
    MATCH_EXACTLY,
    MATCH_BIGGER,
    MATCH_SMALLER
  };

  // Access to the underlying map from icon size->{path, bitmap}.
  typedef std::map<int, std::string> IconMap;

  ExtensionIconSet();
  ~ExtensionIconSet();

  const IconMap& map() const { return map_; }
  bool empty() const { return map_.empty(); }

  // Remove all icons from the set.
  void Clear();

  // Add an icon path to the set. If a path for the specified size is already
  // present, it is overwritten.
  void Add(int size, const std::string& path);

  // Gets path value of the icon found when searching for |size| using
  // |mathc_type|.
  std::string Get(int size, MatchType match_type) const;

  // Returns true iff the set contains the specified path.
  bool ContainsPath(const std::string& path) const;

  // Returns icon size if the set contains the specified path or 0 if not found.
  int GetIconSizeFromPath(const std::string& path) const;

 private:
  IconMap map_;
};

#endif  // EXTENSIONS_COMMON_EXTENSION_ICON_SET_H_
