// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_

class PrefServiceSimple;
class PrefServiceSyncable;

namespace geolocation {
void RegisterPrefs(PrefServiceSimple* prefs);
}

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PREFS_H_
