// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_PREFS_H_
#define APPS_PREFS_H_

class PrefRegistrySimple;

namespace apps {

// Register preferences for the apps system.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace apps

#endif  // APPS_PREFS_H_
