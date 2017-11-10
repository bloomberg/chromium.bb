// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(OomInterventionTabHelper);

namespace {

content::WebContents* g_last_visible_web_contents = nullptr;

bool IsLastVisibleWebContents(content::WebContents* web_contents) {
  return web_contents == g_last_visible_web_contents;
}

void SetLastVisibleWebContents(content::WebContents* web_contents) {
  g_last_visible_web_contents = web_contents;
}

// These enums are associated with UMA. Values must be kept in sync with
// enums.xml and must not be renumbered/reused.
enum class NearOomMonitoringEndReason {
  OOM_PROTECTED_CRASH = 0,
  RENDERER_GONE = 1,
  NAVIGATION = 2,
  COUNT,
};

void RecordNearOomMonitoringEndReason(NearOomMonitoringEndReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Memory.Experimental.OomIntervention.NearOomMonitoringEndReason", reason,
      NearOomMonitoringEndReason::COUNT);
}

void RecordInterventionUserDecision(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("Memory.Experimental.OomIntervention.UserDecision",
                        accepted);
}

void RecordInterventionStateOnCrash(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN(
      "Memory.Experimental.OomIntervention.InterventionStateOnCrash", accepted);
}

const char kRendererPauseParamName[] = "pause_renderer";

bool RendererPauseIsEnabled() {
  static bool enabled = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kRendererPauseParamName, false);
  return enabled;
}

}  // namespace

// static
bool OomInterventionTabHelper::IsEnabled() {
  return NearOomMonitor::GetInstance() != nullptr;
}

OomInterventionTabHelper::OomInterventionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  OutOfMemoryReporter::FromWebContents(web_contents)->AddObserver(this);
}

OomInterventionTabHelper::~OomInterventionTabHelper() = default;

void OomInterventionTabHelper::AcceptIntervention() {
  RecordInterventionUserDecision(true);
  intervention_state_ = InterventionState::ACCEPTED;
}

void OomInterventionTabHelper::DeclineIntervention() {
  RecordInterventionUserDecision(false);
  intervention_.reset();
  intervention_state_ = InterventionState::DECLINED;
}

void OomInterventionTabHelper::WebContentsDestroyed() {
  OutOfMemoryReporter::FromWebContents(web_contents())->RemoveObserver(this);
  StopMonitoring();
}

void OomInterventionTabHelper::RenderProcessGone(
    base::TerminationStatus status) {
  intervention_.reset();

  // Skip background process termination.
  if (!IsLastVisibleWebContents(web_contents())) {
    ResetInterventionState();
    return;
  }

  // OOM crash is handled in OnForegroundOOMDetected().
  if (status == base::TERMINATION_STATUS_OOM_PROTECTED)
    return;

  if (near_oom_detected_time_) {
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "RendererGoneAfterDetectionTime",
        elapsed_time);
    ResetInterventionState();
  } else {
    RecordNearOomMonitoringEndReason(NearOomMonitoringEndReason::RENDERER_GONE);
  }
}

void OomInterventionTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Filter out sub-frame's navigation.
  if (!navigation_handle->IsInMainFrame())
    return;

  // Filter out the first navigation.
  if (!navigation_started_) {
    navigation_started_ = true;
    return;
  }

  // Filter out a navigation if the navigation happens without changing
  // document.
  if (navigation_handle->IsSameDocument())
    return;

  // Filter out background navigation.
  if (!IsLastVisibleWebContents(navigation_handle->GetWebContents())) {
    ResetInterventionState();
    return;
  }

  if (near_oom_detected_time_) {
    // near-OOM was detected.
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "NavigatedAfterDetectionTime",
        elapsed_time);
    ResetInterventionState();
  } else {
    // Monitoring but near-OOM hasn't been detected.
    RecordNearOomMonitoringEndReason(NearOomMonitoringEndReason::NAVIGATION);
  }
}

void OomInterventionTabHelper::DocumentAvailableInMainFrame() {
  if (IsLastVisibleWebContents(web_contents()))
    StartMonitoringIfNeeded();
}

void OomInterventionTabHelper::WasShown() {
  StartMonitoringIfNeeded();
  SetLastVisibleWebContents(web_contents());
}

void OomInterventionTabHelper::WasHidden() {
  StopMonitoring();
}

void OomInterventionTabHelper::OnForegroundOOMDetected(
    const GURL& url,
    ukm::SourceId source_id) {
  DCHECK(IsLastVisibleWebContents(web_contents()));
  if (near_oom_detected_time_) {
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "OomProtectedCrashAfterDetectionTime",
        elapsed_time);

    if (intervention_state_ != InterventionState::NOT_TRIGGERED) {
      // Consider UI_SHOWN as ACCEPTED because we already triggered the
      // intervention and the user didn't decline.
      bool accepted = intervention_state_ != InterventionState::DECLINED;
      RecordInterventionStateOnCrash(accepted);
    }
    ResetInterventionState();
  } else {
    RecordNearOomMonitoringEndReason(
        NearOomMonitoringEndReason::OOM_PROTECTED_CRASH);
  }
}

void OomInterventionTabHelper::StartMonitoringIfNeeded() {
  if (subscription_)
    return;

  if (near_oom_detected_time_)
    return;

  subscription_ = NearOomMonitor::GetInstance()->RegisterCallback(base::Bind(
      &OomInterventionTabHelper::OnNearOomDetected, base::Unretained(this)));
}

void OomInterventionTabHelper::StopMonitoring() {
  subscription_.reset();
}

void OomInterventionTabHelper::OnNearOomDetected() {
  DCHECK(web_contents()->IsVisible());
  DCHECK(!near_oom_detected_time_);
  near_oom_detected_time_ = base::TimeTicks::Now();
  subscription_.reset();

  if (RendererPauseIsEnabled()) {
    content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
    DCHECK(main_frame);
    content::RenderProcessHost* render_process_host = main_frame->GetProcess();
    DCHECK(render_process_host);
    content::BindInterface(render_process_host,
                           mojo::MakeRequest(&intervention_));
    NearOomInfoBar::Show(web_contents(), this);
    intervention_state_ = InterventionState::UI_SHOWN;
  }
}

void OomInterventionTabHelper::ResetInterventionState() {
  near_oom_detected_time_.reset();
  intervention_state_ = InterventionState::NOT_TRIGGERED;
}
