// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_
#define COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_

#include "base/macros.h"

class GURL;

namespace arc {

class LocalActivityResolver {
 public:
  LocalActivityResolver() = default;
  ~LocalActivityResolver() = default;

  bool ShouldChromeHandleUrl(const GURL& url);

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalActivityResolver);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_
