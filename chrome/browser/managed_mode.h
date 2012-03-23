// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_H_
#define CHROME_BROWSER_MANAGED_MODE_H_

class PrefService;
class Profile;

class ManagedMode {
 public:
  static void RegisterPrefs(PrefService* prefs);
  static bool IsInManagedMode();

  // Returns true iff managed mode was entered sucessfully.
  static bool EnterManagedMode(Profile* profile);
  static void LeaveManagedMode();

 private:
  // Platform-specific methods that confirm whether we can enter or leave
  // managed mode.
  static bool PlatformConfirmEnter();
  static bool PlatformConfirmLeave();

  static void SetInManagedMode(bool in_managed_mode);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_H_
