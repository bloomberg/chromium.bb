// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifetime_observer.h"

namespace resource_coordinator {

void TabLifetimeObserver::OnDiscardedStateChange(content::WebContents* contents,
                                                 bool is_discarded) {}

void TabLifetimeObserver::OnAutoDiscardableStateChange(
    content::WebContents* contents,
    bool is_auto_discardable) {}

TabLifetimeObserver::~TabLifetimeObserver() {}

}  // namespace resource_coordinator
