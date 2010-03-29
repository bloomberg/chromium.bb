// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_extent.h"

#include "base/string_util.h"

bool ExtensionExtent::ContainsURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.GetOrigin() != origin_)
    return false;

  if (paths_.empty())
    return true;

  for (size_t i = 0; i < paths_.size(); ++i) {
    if (StartsWithASCII(url.path(),
                        std::string("/") + paths_[i],
                        false)) {  // not case sensitive
      return true;
    }
  }

  return false;
}
