// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_
#define CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_
#pragma once

#include "content/common/renderer_preferences.h"

class Profile;

namespace renderer_preferences_util {

// Copies system configuration preferences into |prefs|.
void UpdateFromSystemSettings(RendererPreferences* prefs, Profile* profile);

}  // namespace renderer_preferences_util

#endif  // CHROME_BROWSER_RENDERER_PREFERENCES_UTIL_H_
