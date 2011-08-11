// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_OBSERVER_H_

// Delegate interface for objects that want to be notified when the
// AvatarMenuModel changes.
class AvatarMenuModelObserver {
 public:
  virtual ~AvatarMenuModelObserver() {}

  virtual void OnAvatarMenuModelChanged() = 0;
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_OBSERVER_H_
