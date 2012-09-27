// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "ui/base/work_area_watcher_observer.h"
#include "ui/base/x/work_area_watcher_x.h"

namespace {

class DisplaySettingsProviderGtk : public DisplaySettingsProvider,
                                   public ui::WorkAreaWatcherObserver {
 public:
  DisplaySettingsProviderGtk();
  virtual ~DisplaySettingsProviderGtk();

 protected:
  // Overridden from ui::WorkAreaWatcherObserver:
  virtual void WorkAreaChanged() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProviderGtk);
};

DisplaySettingsProviderGtk::DisplaySettingsProviderGtk() {
  ui::WorkAreaWatcherX::AddObserver(this);
}

DisplaySettingsProviderGtk::~DisplaySettingsProviderGtk() {
  ui::WorkAreaWatcherX::RemoveObserver(this);
}

void DisplaySettingsProviderGtk::WorkAreaChanged() {
  OnDisplaySettingsChanged();
}

}  // namespace

// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProviderGtk();
}
