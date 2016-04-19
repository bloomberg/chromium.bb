// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_helper.h"

#include <utility>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

namespace {

int g_seconds_to_pause_engagement_detection = 10;
int g_seconds_delay_after_navigation = 10;
int g_seconds_delay_after_media_starts = 10;
int g_seconds_delay_after_show = 5;

}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SiteEngagementHelper);

SiteEngagementHelper::PeriodicTracker::PeriodicTracker(
    SiteEngagementHelper* helper)
    : helper_(helper), pause_timer_(new base::Timer(true, false)) {}

SiteEngagementHelper::PeriodicTracker::~PeriodicTracker() {}

void SiteEngagementHelper::PeriodicTracker::Start(
    base::TimeDelta initial_delay) {
  StartTimer(initial_delay);
}

void SiteEngagementHelper::PeriodicTracker::Pause() {
  TrackingStopped();
  StartTimer(
      base::TimeDelta::FromSeconds(g_seconds_to_pause_engagement_detection));
}

void SiteEngagementHelper::PeriodicTracker::Stop() {
  TrackingStopped();
  pause_timer_->Stop();
}

bool SiteEngagementHelper::PeriodicTracker::IsTimerRunning() {
  return pause_timer_->IsRunning();
}

void SiteEngagementHelper::PeriodicTracker::SetPauseTimerForTesting(
    std::unique_ptr<base::Timer> timer) {
  pause_timer_ = std::move(timer);
}

void SiteEngagementHelper::PeriodicTracker::StartTimer(
    base::TimeDelta delay) {
  pause_timer_->Start(
      FROM_HERE, delay,
      base::Bind(&SiteEngagementHelper::PeriodicTracker::TrackingStarted,
                 base::Unretained(this)));
}

SiteEngagementHelper::InputTracker::InputTracker(
    SiteEngagementHelper* helper,
    content::WebContents* web_contents)
    : PeriodicTracker(helper),
      content::WebContentsObserver(web_contents),
      is_tracking_(false) {}

void SiteEngagementHelper::InputTracker::TrackingStarted() {
  is_tracking_ = true;
}

void SiteEngagementHelper::InputTracker::TrackingStopped() {
  is_tracking_ = false;
}

// Record that there was some user input, and defer handling of the input event.
// Once the timer finishes running, the callbacks detecting user input will be
// registered again.
void SiteEngagementHelper::InputTracker::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  // Only respond to raw key down to avoid multiple triggering on a single input
  // (e.g. keypress is a key down then key up).
  if (!is_tracking_)
    return;

  // This switch has a default NOTREACHED case because it will not test all
  // of the values of the WebInputEvent::Type enum (hence it won't require the
  // compiler verifying that all cases are covered).
  switch (type) {
    case blink::WebInputEvent::RawKeyDown:
      helper()->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_KEYPRESS);
      break;
    case blink::WebInputEvent::MouseDown:
      helper()->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_MOUSE);
      break;
    case blink::WebInputEvent::GestureTapDown:
      helper()->RecordUserInput(
          SiteEngagementMetrics::ENGAGEMENT_TOUCH_GESTURE);
      break;
    case blink::WebInputEvent::GestureScrollBegin:
      helper()->RecordUserInput(SiteEngagementMetrics::ENGAGEMENT_SCROLL);
      break;
    case blink::WebInputEvent::Undefined:
      // Explicitly ignore browser-initiated navigation input.
      break;
    default:
      NOTREACHED();
  }
  Pause();
}

SiteEngagementHelper::MediaTracker::MediaTracker(
    SiteEngagementHelper* helper,
    content::WebContents* web_contents)
    : PeriodicTracker(helper),
      content::WebContentsObserver(web_contents),
      is_hidden_(false) {}

SiteEngagementHelper::MediaTracker::~MediaTracker() {}

void SiteEngagementHelper::MediaTracker::TrackingStarted() {
  if (!active_media_players_.empty())
    helper()->RecordMediaPlaying(is_hidden_);

  Pause();
}

void SiteEngagementHelper::MediaTracker::MediaStartedPlaying(
    const MediaPlayerId& id) {
  // Only begin engagement detection when media actually starts playing.
  active_media_players_.push_back(id);
  if (!IsTimerRunning())
    Start(base::TimeDelta::FromSeconds(g_seconds_delay_after_media_starts));
}

void SiteEngagementHelper::MediaTracker::MediaStoppedPlaying(
    const MediaPlayerId& id) {
  active_media_players_.erase(std::remove(active_media_players_.begin(),
                                          active_media_players_.end(), id),
                              active_media_players_.end());
}

void SiteEngagementHelper::MediaTracker::WasShown() {
  is_hidden_ = false;
}

void SiteEngagementHelper::MediaTracker::WasHidden() {
  is_hidden_ = true;
}

SiteEngagementHelper::~SiteEngagementHelper() {
  content::WebContents* contents = web_contents();
  if (contents) {
    input_tracker_.Stop();
    media_tracker_.Stop();
  }
}

SiteEngagementHelper::SiteEngagementHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      input_tracker_(this, web_contents),
      media_tracker_(this, web_contents),
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

void SiteEngagementHelper::RecordMediaPlaying(bool is_hidden) {
  content::WebContents* contents = web_contents();
  if (contents) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    SiteEngagementService* service =
        SiteEngagementServiceFactory::GetForProfile(profile);

    if (service)
      service->HandleMediaPlaying(contents->GetVisibleURL(), is_hidden);
  }
}

void SiteEngagementHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Ignore in-page navigations. However, do not stop input or media detection.
  if (details.is_in_page)
    return;

  input_tracker_.Stop();
  media_tracker_.Stop();
  record_engagement_ = params.url.SchemeIsHTTPOrHTTPS();

  // Ignore all schemes except HTTP and HTTPS.
  if (!record_engagement_)
    return;

  // Ignore prerender loads. This means that prerenders will not receive
  // navigation engagement. The implications are as follows:
  //
  // - Instant search prerenders from the omnibox trigger DidNavigateMainFrame
  //   twice: once for the prerender, and again when the page swaps in. The
  //   second trigger has transition GENERATED and receives navigation
  //   engagement.
  // - Prerenders initiated by <link rel="prerender"> (e.g. search results) are
  //   always assigned the LINK transition, which is ignored for navigation
  //   engagement.
  //
  // Prerenders trigger WasShown() when they are swapped in, so input engagement
  // will activate even if navigation engagement is not scored.
  if (prerender::PrerenderContents::FromWebContents(web_contents()) != nullptr)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile);

  if (service)
    service->HandleNavigation(params.url, params.transition);

  input_tracker_.Start(
      base::TimeDelta::FromSeconds(g_seconds_delay_after_navigation));
}

void SiteEngagementHelper::WasShown() {
  // Ensure that the input callbacks are registered when we come into view.
  if (record_engagement_) {
    input_tracker_.Start(
        base::TimeDelta::FromSeconds(g_seconds_delay_after_show));
  }
}

void SiteEngagementHelper::WasHidden() {
  // Ensure that the input callbacks are not registered when hidden.
  input_tracker_.Stop();
}

// static
void SiteEngagementHelper::SetSecondsBetweenUserInputCheck(int seconds) {
  g_seconds_to_pause_engagement_detection = seconds;
}

// static
void SiteEngagementHelper::SetSecondsTrackingDelayAfterNavigation(int seconds) {
  g_seconds_delay_after_navigation = seconds;
}

// static
void SiteEngagementHelper::SetSecondsTrackingDelayAfterShow(int seconds) {
  g_seconds_delay_after_show = seconds;
}
