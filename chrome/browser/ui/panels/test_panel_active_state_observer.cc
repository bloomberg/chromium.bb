// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/test_panel_active_state_observer.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/common/chrome_notification_types.h"

PanelActiveStateObserver::PanelActiveStateObserver(
    Panel* panel,
    bool expect_active)
    : panel_(panel),
      expect_active_(expect_active),
      seen_(false),
      running_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
                 content::Source<Panel>(panel));
}

PanelActiveStateObserver::~PanelActiveStateObserver() {}

void PanelActiveStateObserver::Wait() {
  if (seen_ || AtExpectedState())
    return;

  running_ = true;
  message_loop_runner_ = new ui_test_utils::MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

bool PanelActiveStateObserver::AtExpectedState() {
  return panel_->IsActive() == expect_active_;
}

void PanelActiveStateObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!running_)
    return;

  if (AtExpectedState()) {
    seen_ = true;
    message_loop_runner_->Quit();
    running_ = false;
  }
}
