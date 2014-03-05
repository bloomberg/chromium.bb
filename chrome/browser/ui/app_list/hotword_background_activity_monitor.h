// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_MONITOR_H_
#define CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_MONITOR_H_

#include <set>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

#if defined(USE_ASH)
#include "ash/shell_observer.h"
#endif

#if defined(OS_CHROMEOS)
namespace chromeos {
class IdleDetector;
}
#endif

namespace app_list {

class HotwordBackgroundActivityDelegate;

// A monitor to check the user activity and reports the availability of
// background hotword recognizer. Otherwise, the background hotword recognizer
// may conflict with foreground browser activity like voice search in
// Google homepage.
class HotwordBackgroundActivityMonitor
    : public chrome::BrowserListObserver,
      public TabStripModelObserver,
#if defined(USE_ASH)
      public ash::ShellObserver,
#endif
      public MediaCaptureDevicesDispatcher::Observer {
 public:
  explicit HotwordBackgroundActivityMonitor(
      HotwordBackgroundActivityDelegate* delegate);
  virtual ~HotwordBackgroundActivityMonitor();

  bool IsHotwordBackgroundActive();

 private:
  friend class HotwordBackgroundActivityMonitorTest;

  // chrome::BrowserListObserver overrides:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // TabStripModelObserver overrides:
  virtual void TabClosingAt(
      TabStripModel* tab_strip_model,
      content::WebContents* contents,
      int index) OVERRIDE;
  virtual void TabChangedAt(
      content::WebContents* contents,
      int index,
      TabStripModelObserver::TabChangeType change_type) OVERRIDE;

#if defined(USE_ASH)
  // ash::ShellObserver overrides:
  virtual void OnLockStateChanged(bool locked) OVERRIDE;
#endif

  // MediaCaptureDevicesDispatcher::Observer overrides:
  virtual void OnRequestUpdate(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevice& device,
      const content::MediaRequestState state) OVERRIDE;

  // Called when the idle state changed.
  void OnIdleStateChanged(bool is_idle);

  void NotifyActivityChange();

  // Adds/Removes WebContents for the whitelist of tab-recording for testing.
  void AddWebContentsToWhitelistForTest(
      content::WebContents* contents);
  void RemoveWebContentsFromWhitelistForTest(
      content::WebContents* contents);

  HotwordBackgroundActivityDelegate* delegate_;

  bool locked_;
  bool is_idle_;
  std::set<int> recording_renderer_ids_;
  std::set<content::WebContents*> recording_contents_for_test_;

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::IdleDetector> idle_detector_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HotwordBackgroundActivityMonitor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_HOTWORD_BACKGROUND_ACTIVITY_MONITOR_H_
