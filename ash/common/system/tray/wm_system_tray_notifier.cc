// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/wm_system_tray_notifier.h"

#include "ash/common/system/update/update_observer.h"

namespace ash {

WmSystemTrayNotifier::WmSystemTrayNotifier() {}

WmSystemTrayNotifier::~WmSystemTrayNotifier() {}

void WmSystemTrayNotifier::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyUpdateRecommended(const UpdateInfo& info) {
  FOR_EACH_OBSERVER(UpdateObserver, update_observers_,
                    OnUpdateRecommended(info));
}

}  // namespace ash
