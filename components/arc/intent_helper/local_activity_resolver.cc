// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/local_activity_resolver.h"

#include "url/gurl.h"

namespace arc {

bool LocalActivityResolver::ShouldChromeHandleUrl(const GURL& url) {
  // Stub implementation for now.
  return true;
}

}  // namespace arc
