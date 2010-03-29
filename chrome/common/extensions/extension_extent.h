// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

// Represents the set of URLs an extension uses for web content.
class ExtensionExtent {
 public:
  const std::vector<std::string>& paths() const { return paths_; }
  void add_path(const std::string& val) { paths_.push_back(val); }
  void clear_paths() { paths_.clear(); }

  const GURL& origin() const { return origin_; }
  void set_origin(const GURL& val) { origin_ = val; }

  bool ContainsURL(const GURL& url);

 private:
  // The security origin (scheme+host+port) of the extent.
  GURL origin_;

  // A set of path prefixes that further restrict the set of valid URLs below
  // origin_. This may be empty.
  std::vector<std::string> paths_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
