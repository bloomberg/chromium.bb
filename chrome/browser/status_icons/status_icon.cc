// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon.h"

void StatusIcon::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StatusIcon::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StatusIcon::DispatchClickEvent() {
  FOR_EACH_OBSERVER(Observer, observers_, OnClicked());
}
