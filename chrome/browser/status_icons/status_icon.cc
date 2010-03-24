// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/status_icons/status_icon.h"

void StatusIcon::AddObserver(StatusIconObserver* observer) {
  observers_.push_back(observer);
}

void StatusIcon::RemoveObserver(StatusIconObserver* observer) {
  std::vector<StatusIconObserver*>::iterator iter =
      std::find(observers_.begin(), observers_.end(), observer);
  if (iter != observers_.end())
    observers_.erase(iter);
}

void StatusIcon::DispatchClickEvent() {
  // Walk observers, call callback for each one.
  for (std::vector<StatusIconObserver*>::const_iterator iter =
           observers_.begin();
       iter != observers_.end();
       ++iter) {
    StatusIconObserver* observer = *iter;
    observer->OnClicked();
  }
}

