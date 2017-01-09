// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_manager.h"

namespace chromeos {

ScreenManager::ScreenManager() {
}

ScreenManager::~ScreenManager() {
}

BaseScreen* ScreenManager::GetScreen(OobeScreen screen) {
  auto iter = screens_.find(screen);
  if (iter != screens_.end())
    return iter->second.get();

  BaseScreen* result = CreateScreen(screen);
  DCHECK(result) << "Can not create screen named " << GetOobeScreenName(screen);
  screens_[screen] = make_linked_ptr(result);
  return result;
}

bool ScreenManager::HasScreen(OobeScreen screen) {
  return screens_.count(screen) > 0;
}

}  // namespace chromeos
