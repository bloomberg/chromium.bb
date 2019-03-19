// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_manager.h"

#include "chrome/browser/web_applications/components/install_manager_observer.h"

namespace web_app {

InstallManager::InstallManager() = default;

InstallManager::~InstallManager() = default;

void InstallManager::Reset() {
  for (InstallManagerObserver& observer : observers_)
    observer.InstallManagerReset();
}

void InstallManager::AddObserver(InstallManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void InstallManager::RemoveObserver(InstallManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace web_app
