// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_helper.h"

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace {

double g_seconds_between_user_input_check = 10;

}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SiteEngagementHelper);

SiteEngagementHelper::InputTracker::InputTracker(SiteEngagementHelper* helper)
    : helper_(helper),
      pause_timer_(new base::Timer(true, false)),
      callbacks_added_(false) {
  key_press_event_callback_ =
      base::Bind(&SiteEngagementHelper::InputTracker::HandleKeyPressEvent,
                 base::Unretained(this));
  mouse_event_callback_ =
      base::Bind(&SiteEngagementHelper::InputTracker::HandleMouseEvent,
                 base::Unretained(this));
}

SiteEngagementHelper::InputTracker::~InputTracker() { }

// Record that there was some user input, and defer handling of the input event.
// web_contents() will return nullptr if the observed contents have been
// deleted; if the contents exist, record engagement for the site. Once the
// timer finishes running, the callbacks detecting user input will be registered
// again.
bool SiteEngagementHelper::InputTracker::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Only respond to raw key down to avoid multiple triggering on a single input
  // (e.g. keypress is a key down then key up).
  if (event.type == blink::WebInputEvent::RawKeyDown) {
    PauseTracking(helper_->web_contents()->GetRenderViewHost());
    helper_->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
  }
  return false;
}

bool SiteEngagementHelper::InputTracker::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  // Only respond to mouse down with a button or mouse move events (e.g. a click
  // is a mouse down and mouse up) to avoid cases where multiple events come in
  // before we can pause tracking.
  if ((event.button != blink::WebMouseEvent::ButtonNone &&
       event.type == blink::WebInputEvent::MouseDown) ||
      event.type == blink::WebInputEvent::MouseWheel) {
    PauseTracking(helper_->web_contents()->GetRenderViewHost());
    helper_->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  }
  return false;
}

void SiteEngagementHelper::InputTracker::StartTracking(
    content::RenderViewHost* host) {
  if (!callbacks_added_) {
    host->AddKeyPressEventCallback(key_press_event_callback_);
    host->AddMouseEventCallback(mouse_event_callback_);
    callbacks_added_ = true;
  }
}

void SiteEngagementHelper::InputTracker::PauseTracking(
    content::RenderViewHost* host) {
  StopTracking(host);
  pause_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(g_seconds_between_user_input_check),
      base::Bind(&SiteEngagementHelper::InputTracker::ResumeTracking,
                 base::Unretained(this)));
}

void SiteEngagementHelper::InputTracker::ResumeTracking() {
  content::WebContents* contents = helper_->web_contents();
  if (contents)
    StartTracking(contents->GetRenderViewHost());
}

void SiteEngagementHelper::InputTracker::StopTracking(
    content::RenderViewHost* host) {
  pause_timer_->Stop();
  if (callbacks_added_) {
    host->RemoveKeyPressEventCallback(key_press_event_callback_);
    host->RemoveMouseEventCallback(mouse_event_callback_);
    callbacks_added_ = false;
  }
}

void SiteEngagementHelper::InputTracker::SetTimerForTesting(
    scoped_ptr<base::Timer> timer) {
  pause_timer_ = timer.Pass();
}

SiteEngagementHelper::~SiteEngagementHelper() {
  content::WebContents* contents = web_contents();
  if (contents)
    input_tracker_.StopTracking(contents->GetRenderViewHost());
}

SiteEngagementHelper::SiteEngagementHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      input_tracker_(this),
      record_engagement_(false) { }

void SiteEngagementHelper::RecordUserInput(
    SiteEngagementMetrics::EngagementType type) {
  TRACE_EVENT0("SiteEngagement", "RecordUserInput");
  content::WebContents* contents = web_contents();
  if (contents) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    SiteEngagementService* service =
        SiteEngagementServiceFactory::GetForProfile(profile);

    // Service is null in incognito.
    if (service)
      service->HandleUserInput(contents->GetVisibleURL(), type);
  }
}

bool SiteEngagementHelper::ShouldRecordEngagement() {
  return record_engagement_;
}

void SiteEngagementHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  content::WebContents* contents = web_contents();
  input_tracker_.StopTracking(contents->GetRenderViewHost());

  // Ignore all schemes except HTTP and HTTPS.
  record_engagement_ = params.url.SchemeIsHTTPOrHTTPS();

  if (!ShouldRecordEngagement())
    return;

  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile);

  if (service)
    service->HandleNavigation(params.url, params.transition);

  input_tracker_.StartTracking(contents->GetRenderViewHost());
}

void SiteEngagementHelper::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  // On changing the render view host, we need to re-register the callbacks
  // listening for user input.
  if (ShouldRecordEngagement()) {
    if (old_host)
      input_tracker_.StopTracking(old_host);
    input_tracker_.StartTracking(new_host);
  }
}

void SiteEngagementHelper::WasShown() {
  // Ensure that the input callbacks are registered when we come into view.
  if (ShouldRecordEngagement())
    input_tracker_.StartTracking(web_contents()->GetRenderViewHost());
}

void SiteEngagementHelper::WasHidden() {
  // Ensure that the input callbacks are not registered when hidden.
  if (ShouldRecordEngagement()) {
    content::WebContents* contents = web_contents();
    if (contents)
      input_tracker_.StopTracking(contents->GetRenderViewHost());
  }
}

// static
void SiteEngagementHelper::SetSecondsBetweenUserInputCheck(double seconds) {
  g_seconds_between_user_input_check = seconds;
}
