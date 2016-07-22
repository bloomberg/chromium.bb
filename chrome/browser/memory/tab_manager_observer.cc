// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_observer.h"

namespace content {
class WebContents;
}

namespace memory {

void TabManagerObserver::OnDiscardedStateChange(content::WebContents* contents,
                                                bool is_discarded) {}

void TabManagerObserver::OnAutoDiscardableStateChange(
    content::WebContents* contents,
    bool is_auto_discardable) {}

TabManagerObserver::~TabManagerObserver() {}

}  // namespace memory
