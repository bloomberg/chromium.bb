// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_PATH_NAME_CMP_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_PATH_NAME_CMP_H_

#include <string>

#include "chrome/browser/sync/util/sync_types.h"

namespace syncable {

struct LessPathNames {
  bool operator() (const std::string&, const std::string&) const;
};

int ComparePathNames(const std::string& a, const std::string& b);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_PATH_NAME_CMP_H_
