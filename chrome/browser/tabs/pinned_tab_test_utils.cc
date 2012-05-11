// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_test_utils.h"

namespace {

std::string TabToString(const StartupTab& tab) {
  return tab.url.spec() + ":" + (tab.is_app ? "app" : "") + ":" +
      (tab.is_pinned ? "pinned" : "") + ":" + tab.app_id;
}

}  // namespace

// static
std::string PinnedTabTestUtils::TabsToString(
    const std::vector<StartupTab>& values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0)
      result += " ";
    result += TabToString(values[i]);
  }
  return result;
}
