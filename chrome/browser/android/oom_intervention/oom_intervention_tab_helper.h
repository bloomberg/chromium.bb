// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"
#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"
#include "chrome/browser/ui/android/infobars/near_oom_infobar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/oom_intervention.mojom.h"

namespace content {
class WebContents;
}

// A tab helper for near-OOM intervention. This class depends on
// OutOfMemoryReporter. OutOfMemoryReporter must be created on TabHelpers
// before creating OomInterventionTabHelper.
class OomInterventionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OomInterventionTabHelper>,
      public OutOfMemoryReporter::Observer,
      public NearOomMessageDelegate {
 public:
  static bool IsEnabled();

  ~OomInterventionTabHelper() override;

  // NearOomMessageDelegate:
  void AcceptIntervention() override;
  void DeclineIntervention() override;

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

  // OutOfMemoryReporter::Observer:
  void OnForegroundOOMDetected(const GURL& url,
                               ukm::SourceId source_id) override;

  // Starts observing near-OOM situation if it's not started.
  void StartMonitoringIfNeeded();
  // Stops observing near-OOM situation.
  void StopMonitoring();

  // Called when NearOomMonitor detects near-OOM situation.
  void OnNearOomDetected();

  void ResetInterventionState();

  bool navigation_started_ = false;
  base::Optional<base::TimeTicks> near_oom_detected_time_;
  std::unique_ptr<NearOomMonitor::Subscription> subscription_;

  blink::mojom::OomInterventionPtr intervention_;

  enum class InterventionState {
    // Intervention isn't triggered yet.
    NOT_TRIGGERED,
    // Intervention is triggered but the user doesn't respond yet.
    UI_SHOWN,
    // Intervention is triggered and the user declined it.
    DECLINED,
    // Intervention is triggered and the user accepted it.
    ACCEPTED,
  };

  InterventionState intervention_state_ = InterventionState::NOT_TRIGGERED;
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
