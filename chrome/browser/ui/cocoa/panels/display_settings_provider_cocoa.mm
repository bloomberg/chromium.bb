// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "ui/base/work_area_watcher_observer.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {

class DisplaySettingsProviderCocoa : public DisplaySettingsProvider,
                                     public ui::WorkAreaWatcherObserver,
                                     public content::NotificationObserver {
 public:
  DisplaySettingsProviderCocoa();
  virtual ~DisplaySettingsProviderCocoa();

 protected:
  // Overridden from DisplaySettingsProvider:
  virtual bool NeedsPeriodicFullScreenCheck() const OVERRIDE;
  virtual bool IsFullScreen() const OVERRIDE;

  // Overridden from ui::WorkAreaWatcherObserver:
  virtual void WorkAreaChanged() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  bool is_chrome_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProviderCocoa);
};

DisplaySettingsProviderCocoa::DisplaySettingsProviderCocoa()
    // Ideally we should initialize the value below with current state. However,
    // there is not a centralized place we can pull the state from and we need
    // to query all chrome browser windows to find it out.
    // This will get automatically fixed when DisplaySettingsProvider is changed
    // to be initialized before any chrome window is created with planning
    // refactor effort to move it out of panel scope.
    : is_chrome_fullscreen_(false) {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController addObserverForWorkAreaChange:this];

  registrar_.Add(
      this,
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::NotificationService::AllSources());
}

DisplaySettingsProviderCocoa::~DisplaySettingsProviderCocoa() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController removeObserverForWorkAreaChange:this];
}

bool DisplaySettingsProviderCocoa::NeedsPeriodicFullScreenCheck() const {
  // Lion system introduces fullscreen support. When a window of an application
  // enters fullscreen mode, the system will automatically hide all other
  // windows, even including topmost windows that come from other applications.
  // So we don't need to do anything when any other application enters
  // fullscreen mode. We still need to handle the case when chrome enters
  // fullscreen mode and our topmost windows will not get hided by the system.
  return !base::mac::IsOSLionOrLater();
}

bool DisplaySettingsProviderCocoa::IsFullScreen() const {
  // For Lion and later, we only need to check if chrome enters fullscreen mode
  // (see detailed reason above in NeedsPeriodicFullScreenCheck).
  return base::mac::IsOSLionOrLater() ? is_chrome_fullscreen_ :
      DisplaySettingsProvider::IsFullScreen();
}

void DisplaySettingsProviderCocoa::WorkAreaChanged() {
  OnDisplaySettingsChanged();
}

void DisplaySettingsProviderCocoa::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  is_chrome_fullscreen_ = *(content::Details<bool>(details)).ptr();
  CheckFullScreenMode();
}

}  // namespace

// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProviderCocoa();
}
