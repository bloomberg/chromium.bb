// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_
#pragma once

class PrefService;

namespace geolocation {
void RegisterPrefs(PrefService* prefs);
void RegisterUserPrefs(PrefService* user_prefs);
}

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_
