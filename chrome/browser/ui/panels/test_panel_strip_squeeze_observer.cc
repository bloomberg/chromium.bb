// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/test_panel_strip_squeeze_observer.h"

#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"

PanelStripSqueezeObserver::PanelStripSqueezeObserver(
    DockedPanelStrip* strip, Panel* active_panel)
    : TestPanelNotificationObserver(
        chrome::NOTIFICATION_PANEL_STRIP_UPDATED,
        content::Source<PanelStrip>(strip)),
      panel_strip_(strip),
      active_panel_(active_panel) {
}

PanelStripSqueezeObserver::~PanelStripSqueezeObserver() {}

bool PanelStripSqueezeObserver::IsSqueezed(Panel* panel) {
  return panel->GetBounds().width() < panel->GetRestoredBounds().width();
}

bool PanelStripSqueezeObserver::AtExpectedState() {
  const DockedPanelStrip::Panels& panels = panel_strip_->panels();
  for (DockedPanelStrip::Panels::const_iterator iter = panels.begin();
       iter != panels.end(); ++iter) {
    if (*iter == active_panel_) {
      if (IsSqueezed(*iter))
        return false;
    } else if (!IsSqueezed(*iter)) {
      return false;
    }
  }
  return true;
}
