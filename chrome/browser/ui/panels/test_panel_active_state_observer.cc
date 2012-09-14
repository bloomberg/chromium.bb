// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/test_panel_active_state_observer.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"

PanelActiveStateObserver::PanelActiveStateObserver(
    Panel* panel,
    bool expect_active)
    : TestPanelNotificationObserver(
        chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
        content::Source<Panel>(panel)),
      panel_(panel),
      expect_active_(expect_active) {
}

PanelActiveStateObserver::~PanelActiveStateObserver() {}

bool PanelActiveStateObserver::AtExpectedState() {
  return panel_->IsActive() == expect_active_;
}
