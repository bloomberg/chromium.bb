// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_OPT_IN_H_
#define CHROME_BROWSER_INSTANT_INSTANT_OPT_IN_H_
#pragma once

#include "gfx/native_widget_types.h"

class Profile;

namespace browser {

// Returns true if the opt-in should be shown.
bool ShouldShowInstantOptIn(Profile* profile);

// Invoked if the user clicks on the opt-in promo.
void UserPickedInstantOptIn(gfx::NativeWindow parent,
                            Profile* profile,
                            bool opt_in);

}  // namespace browser

#endif  // CHROME_BROWSER_INSTANT_INSTANT_OPT_IN_H_
