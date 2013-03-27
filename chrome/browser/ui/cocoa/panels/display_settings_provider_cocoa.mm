// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/mac/mac_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/work_area_watcher_observer.h"

namespace {

// The time, in milliseconds, that a fullscreen check will be started after
// the active workspace change is notified. This value is from experiment.
const int kCheckFullScreenDelayTimeMs = 200;

class DisplaySettingsProviderCocoa : public DisplaySettingsProvider,
                                     public ui::WorkAreaWatcherObserver,
                                     public content::NotificationObserver {
 public:
  DisplaySettingsProviderCocoa();
  virtual ~DisplaySettingsProviderCocoa();

  void ActiveSpaceChanged();

 protected:
  // Overridden from DisplaySettingsProvider:
  virtual bool NeedsPeriodicFullScreenCheck() const OVERRIDE;
  virtual bool IsFullScreen() OVERRIDE;

  // Overridden from ui::WorkAreaWatcherObserver:
  virtual void WorkAreaChanged() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void ActiveWorkSpaceChanged();

  content::NotificationRegistrar registrar_;
  id active_space_change_;

  // Owned by MessageLoop after posting.
  base::WeakPtrFactory<DisplaySettingsProviderCocoa> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySettingsProviderCocoa);
};

DisplaySettingsProviderCocoa::DisplaySettingsProviderCocoa()
    : active_space_change_(nil),
      weak_factory_(this) {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController addObserverForWorkAreaChange:this];

  registrar_.Add(
      this,
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::NotificationService::AllSources());

  active_space_change_ = [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification* notification) {
                  ActiveWorkSpaceChanged();
              }];
}

DisplaySettingsProviderCocoa::~DisplaySettingsProviderCocoa() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController removeObserverForWorkAreaChange:this];

  [[[NSWorkspace sharedWorkspace] notificationCenter]
      removeObserver:active_space_change_];
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

bool DisplaySettingsProviderCocoa::IsFullScreen() {
  // For Lion and later, we only need to check if chrome enters fullscreen mode
  // (see detailed reason above in NeedsPeriodicFullScreenCheck).
  if (!base::mac::IsOSLionOrLater())
    return DisplaySettingsProvider::IsFullScreen();

  Browser* browser = chrome::GetLastActiveBrowser();
  if (!browser)
    return false;
  BrowserWindow* browser_window = browser->window();
  if (!browser_window->IsFullscreen())
    return false;

  // If the user switches to another space where the fullscreen browser window
  // does not live, we do not call it fullscreen.
  NSWindow* native_window = browser_window->GetNativeWindow();
  return [native_window isOnActiveSpace];
}

void DisplaySettingsProviderCocoa::WorkAreaChanged() {
  OnDisplaySettingsChanged();
}

void DisplaySettingsProviderCocoa::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  // When we receive the fullscreen notification, the Chrome Window has not been
  // put on the active space yet and thus IsFullScreen will return false.
  // Since the fullscreen result is already known here, we can pass it dierctly
  // to CheckFullScreenMode.
  bool is_fullscreen = *(content::Details<bool>(details)).ptr();
  CheckFullScreenMode(
      is_fullscreen ? ASSUME_FULLSCREEN_ON : ASSUME_FULLSCREEN_OFF);
}

void DisplaySettingsProviderCocoa::ActiveWorkSpaceChanged() {
  // The active workspace notification might be received earlier than the
  // browser window knows that it is not in active space.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DisplaySettingsProviderCocoa::CheckFullScreenMode,
                 weak_factory_.GetWeakPtr(),
                 PERFORM_FULLSCREEN_CHECK),
      base::TimeDelta::FromMilliseconds(kCheckFullScreenDelayTimeMs));
}

}  // namespace

// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProviderCocoa();
}
