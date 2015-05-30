// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/test/test_chrome_launcher_app_menu_item.h"

#include "base/strings/utf_string_conversions.h"

TestChromeLauncherAppMenuItem::TestChromeLauncherAppMenuItem()
    : ChromeLauncherAppMenuItem(base::ASCIIToUTF16("DummyTitle"),
                                nullptr,
                                false) {
}

TestChromeLauncherAppMenuItem::~TestChromeLauncherAppMenuItem() {
}

bool TestChromeLauncherAppMenuItem::IsActive() const {
  return is_active_;
}

bool TestChromeLauncherAppMenuItem::IsEnabled() const {
  return is_enabled_;
}

void TestChromeLauncherAppMenuItem::Execute(int event_flags) {
  ++execute_count_;
}
