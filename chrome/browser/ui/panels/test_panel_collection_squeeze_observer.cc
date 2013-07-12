// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/test_panel_collection_squeeze_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/panel.h"
#include "content/public/browser/notification_source.h"

PanelCollectionSqueezeObserver::PanelCollectionSqueezeObserver(
    DockedPanelCollection* collection, Panel* active_panel)
    : TestPanelNotificationObserver(
        chrome::NOTIFICATION_PANEL_COLLECTION_UPDATED,
        content::Source<PanelCollection>(collection)),
      panel_collection_(collection),
      active_panel_(active_panel) {
}

PanelCollectionSqueezeObserver::~PanelCollectionSqueezeObserver() {}

bool PanelCollectionSqueezeObserver::IsSqueezed(Panel* panel) {
  return panel->GetBounds().width() < panel->GetRestoredBounds().width();
}

bool PanelCollectionSqueezeObserver::AtExpectedState() {
  const DockedPanelCollection::Panels& panels = panel_collection_->panels();
  for (DockedPanelCollection::Panels::const_iterator iter = panels.begin();
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
