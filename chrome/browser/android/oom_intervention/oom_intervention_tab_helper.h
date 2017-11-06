// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// A tab helper for near-OOM intervention.
class OomInterventionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OomInterventionTabHelper> {
 public:
  static bool IsEnabled();

  ~OomInterventionTabHelper() override;

 private:
  explicit OomInterventionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<OomInterventionTabHelper>;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void WasShown() override;
  void WasHidden() override;

  // Starts observing near-OOM situation if it's not started.
  void StartMonitoringIfNeeded();
  // Stops observing near-OOM situation.
  void StopMonitoring();

  // Called when NearOomMonitor detects near-OOM situation.
  void OnNearOomDetected();

  bool navigation_started_ = false;
  base::Optional<base::TimeTicks> near_oom_detected_time_;
  std::unique_ptr<NearOomMonitor::Subscription> subscription_;
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
