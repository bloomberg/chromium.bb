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

int g_seconds_between_user_input_check = 10;
int g_seconds_tracking_delay_after_navigation = 10;
int g_seconds_tracking_delay_after_show = 5;

}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SiteEngagementHelper);

SiteEngagementHelper::InputTracker::InputTracker(SiteEngagementHelper* helper)
    : helper_(helper),
      pause_timer_(new base::Timer(true, false)),
      host_(nullptr),
      is_active_(false),
      is_tracking_(false) {
  key_press_event_callback_ =
      base::Bind(&SiteEngagementHelper::InputTracker::HandleKeyPressEvent,
                 base::Unretained(this));
  mouse_event_callback_ =
      base::Bind(&SiteEngagementHelper::InputTracker::HandleMouseEvent,
                 base::Unretained(this));
}

SiteEngagementHelper::InputTracker::~InputTracker() {}

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
    Pause();
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
    Pause();
    helper_->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_MOUSE);
  }
  return false;
}

void SiteEngagementHelper::InputTracker::Start(content::RenderViewHost* host,
                                               base::TimeDelta initial_delay) {
  DCHECK(!is_active_);
  DCHECK(host);
  host_ = host;
  StartTimer(initial_delay);
  is_active_ = true;
}

void SiteEngagementHelper::InputTracker::Pause() {
  RemoveCallbacks();
  StartTimer(base::TimeDelta::FromSeconds(g_seconds_between_user_input_check));
}

void SiteEngagementHelper::InputTracker::SwitchRenderViewHost(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  DCHECK(is_tracking_);
  DCHECK(new_host);

  bool was_tracking = is_tracking_;
  if (old_host) {
    DCHECK_EQ(host_, old_host);
    RemoveCallbacks();
  }

  host_ = new_host;

  if (was_tracking)
    AddCallbacks();
}

void SiteEngagementHelper::InputTracker::Stop() {
  pause_timer_->Stop();
  RemoveCallbacks();
  host_ = nullptr;
  is_active_ = false;
}

void SiteEngagementHelper::InputTracker::SetPauseTimerForTesting(
    scoped_ptr<base::Timer> timer) {
  pause_timer_ = timer.Pass();
}

void SiteEngagementHelper::InputTracker::StartTimer(base::TimeDelta delay) {
  pause_timer_->Start(
      FROM_HERE, delay,
      base::Bind(&SiteEngagementHelper::InputTracker::AddCallbacks,
                 base::Unretained(this)));
}

void SiteEngagementHelper::InputTracker::AddCallbacks() {
  content::WebContents* contents = helper_->web_contents();
  if (!contents)
    return;

  host_->GetWidget()->AddKeyPressEventCallback(key_press_event_callback_);
  host_->GetWidget()->AddMouseEventCallback(mouse_event_callback_);
  is_tracking_ = true;
}

void SiteEngagementHelper::InputTracker::RemoveCallbacks() {
  if (is_tracking_) {
    host_->GetWidget()->RemoveKeyPressEventCallback(key_press_event_callback_);
    host_->GetWidget()->RemoveMouseEventCallback(mouse_event_callback_);
    is_tracking_ = false;
  }
}

SiteEngagementHelper::~SiteEngagementHelper() {
  content::WebContents* contents = web_contents();
  if (contents)
    input_tracker_.Stop();
}

SiteEngagementHelper::SiteEngagementHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      input_tracker_(this),
      record_engagement_(false) {}

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

void SiteEngagementHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  input_tracker_.Stop();

  record_engagement_ = params.url.SchemeIsHTTPOrHTTPS();

  // Ignore all schemes except HTTP and HTTPS.
  if (!record_engagement_)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile);

  if (service)
    service->HandleNavigation(params.url, params.transition);

  input_tracker_.Start(
      web_contents()->GetRenderViewHost(),
      base::TimeDelta::FromSeconds(g_seconds_tracking_delay_after_navigation));
}

void SiteEngagementHelper::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  // On changing the render view host, we need to re-register the callbacks
  // listening for user input.
  if (input_tracker_.is_tracking()) {
    input_tracker_.SwitchRenderViewHost(old_host, new_host);
  }
}

void SiteEngagementHelper::WasShown() {
  // Ensure that the input callbacks are registered when we come into view.
  if (record_engagement_) {
    input_tracker_.Start(
        web_contents()->GetRenderViewHost(),
        base::TimeDelta::FromSeconds(g_seconds_tracking_delay_after_show));
  }
}

void SiteEngagementHelper::WasHidden() {
  // Ensure that the input callbacks are not registered when hidden.
  input_tracker_.Stop();
}

// static
void SiteEngagementHelper::SetSecondsBetweenUserInputCheck(int seconds) {
  g_seconds_between_user_input_check = seconds;
}

// static
void SiteEngagementHelper::SetSecondsTrackingDelayAfterNavigation(int seconds) {
  g_seconds_tracking_delay_after_navigation = seconds;
}

// static
void SiteEngagementHelper::SetSecondsTrackingDelayAfterShow(int seconds) {
  g_seconds_tracking_delay_after_show = seconds;
}
