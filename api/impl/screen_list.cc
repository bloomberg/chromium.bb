// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_list.h"

#include <algorithm>

namespace openscreen {

ScreenList::ScreenList() = default;
ScreenList::~ScreenList() = default;

void ScreenList::OnScreenAdded(const ScreenInfo& info) {
  screens_.emplace_back(info);
}

bool ScreenList::OnScreenChanged(const ScreenInfo& info) {
  auto existing_info = std::find_if(
      screens_.begin(), screens_.end(),
      [&info](const ScreenInfo& x) { return x.screen_id == info.screen_id; });
  if (existing_info == screens_.end()) {
    return false;
  }
  *existing_info = info;
  return true;
}

bool ScreenList::OnScreenRemoved(const ScreenInfo& info) {
  const auto it = std::remove(screens_.begin(), screens_.end(), info);
  if (it == screens_.end())
    return false;
  screens_.erase(it, screens_.end());
  return true;
}

bool ScreenList::OnAllScreensRemoved() {
  const auto empty = screens_.empty();
  screens_.clear();
  return !empty;
}

}  // namespace openscreen
