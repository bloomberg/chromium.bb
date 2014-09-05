// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_manager.h"

namespace chromeos {

ScreenManager::ScreenManager() {
}

ScreenManager::~ScreenManager() {
}

WizardScreen* ScreenManager::GetScreen(const std::string& screen_name) {
  ScreenMap::const_iterator iter = screens_.find(screen_name);
  if (iter != screens_.end()) {
    return iter->second.get();
  }
  WizardScreen* result = CreateScreen(screen_name);
  DCHECK(result) << "Can not create screen named " << screen_name;
  screens_[screen_name] = make_linked_ptr(result);
  return result;
}

bool ScreenManager::HasScreen(const std::string& screen_name) {
  return screens_.count(screen_name) > 0;
}

}  // namespace chromeos
