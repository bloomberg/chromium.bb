// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "ui/base/work_area_watcher_observer.h"
#import "chrome/browser/app_controller_mac.h"

namespace {

class DisplaySettingsProviderCocoa : public DisplaySettingsProvider,
                                     public ui::WorkAreaWatcherObserver {
 public:
  DisplaySettingsProviderCocoa();
  virtual ~DisplaySettingsProviderCocoa();

 protected:
  // Overridden from ui::WorkAreaWatcherObserver:
  virtual void WorkAreaChanged() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProviderCocoa);
};

DisplaySettingsProviderCocoa::DisplaySettingsProviderCocoa() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController addObserverForWorkAreaChange:this];
}

DisplaySettingsProviderCocoa::~DisplaySettingsProviderCocoa() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController removeObserverForWorkAreaChange:this];
}

void DisplaySettingsProviderCocoa::WorkAreaChanged() {
  OnDisplaySettingsChanged();
}

}  // namespace

// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProviderCocoa();
}
