// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
#define CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_

#include <set>

#include "base/callback.h"

// Callback used by the bridge and all the downstream classes that propagate
// the callback to get displayed notifications.
using GetDisplayedNotificationsCallback =
    base::Callback<void(std::unique_ptr<std::set<std::string>>,
                        bool /* supports_synchronization */)>;

#endif  // CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
