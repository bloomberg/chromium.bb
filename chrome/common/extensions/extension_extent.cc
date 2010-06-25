// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_extent.h"

bool ExtensionExtent::ContainsURL(const GURL& url) const {
  for (PatternList::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}
