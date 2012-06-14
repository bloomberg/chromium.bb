// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

namespace captive_portal {

namespace {

// The time to wait for a slow loading SSL page before triggering a captive
// portal check.
const int kDefaultSlowSSLTimeSeconds = 30;

}  // namespace

CaptivePortalTabReloader::CaptivePortalTabReloader(
    Profile* profile,
    content::WebContents* web_contents,
    const OpenLoginTabCallback& open_login_tab_callback)
    : profile_(profile),
      web_contents_(web_contents),
      state_(STATE_NONE),
      provisional_main_frame_load_(false),
      slow_ssl_load_time_(
          base::TimeDelta::FromSeconds(kDefaultSlowSSLTimeSeconds)),
      open_login_tab_callback_(open_login_tab_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

CaptivePortalTabReloader::~CaptivePortalTabReloader() {
}

void CaptivePortalTabReloader::OnLoadStart(bool is_ssl) {
  provisional_main_frame_load_ = true;

  SetState(STATE_NONE);

  // Start the slow load timer for SSL pages.
  // TODO(mmenke):  Should this look at the port instead?  The reason the
  //                request never connects is because of the port, not the
  //                protocol.
  if (is_ssl)
    SetState(STATE_TIMER_RUNNING);
}

void CaptivePortalTabReloader::OnLoadCommitted(int net_error) {
  provisional_main_frame_load_ = false;

  if (state_ == STATE_NONE)
    return;

  // If there's no timeout error, reset the state.
  if (net_error != net::ERR_CONNECTION_TIMED_OUT) {
    // TODO(mmenke):  If the new URL is the same as the old broken URL, and the
    //                request succeeds, should probably trigger another
    //                captive portal check.
    SetState(STATE_NONE);
    return;
  }

  // The page timed out before the timer triggered.  This is not terribly
  // likely, but if it does happen, the tab may have been broken by a captive
  // portal.  Go ahead and try to detect a portal now, rather than waiting for
  // the timer.
  if (state_ == STATE_TIMER_RUNNING) {
    OnSlowSSLConnect();
    return;
  }

  // If the tab needs to reload, do so asynchronously, to avoid reentrancy
  // issues.
  if (state_ == STATE_NEEDS_RELOAD) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CaptivePortalTabReloader::ReloadTabIfNeeded,
                   weak_factory_.GetWeakPtr()));
  }
}

void CaptivePortalTabReloader::OnAbort() {
  provisional_main_frame_load_ = false;
  SetState(STATE_NONE);
}

void CaptivePortalTabReloader::OnCaptivePortalResults(
    Result previous_result,
    Result result) {
  if (result == RESULT_BEHIND_CAPTIVE_PORTAL) {
    if (state_ == STATE_MAYBE_BROKEN_BY_PORTAL) {
      SetState(STATE_BROKEN_BY_PORTAL);
      MaybeOpenCaptivePortalLoginTab();
    }
    return;
  }

  switch (state_) {
    case STATE_MAYBE_BROKEN_BY_PORTAL:
    case STATE_TIMER_RUNNING:
      // If the previous result was BEHIND_CAPTIVE_PORTAL, and the state is
      // either STATE_MAYBE_BROKEN_BY_PORTAL or STATE_TIMER_RUNNING, reload the
      // tab.  In the latter case, the tab has yet to commit, but is an SSL
      // page, so if the page ends up at a timeout error, it will be reloaded.
      // If not, the state will just be reset.  The helps in the case that a
      // user tries to reload a tab, and then quickly logs in.
      if (previous_result == RESULT_BEHIND_CAPTIVE_PORTAL) {
        SetState(STATE_NEEDS_RELOAD);
        return;
      }
      SetState(STATE_NONE);
      return;

    case STATE_BROKEN_BY_PORTAL:
      // Either reload the tab now, if a connection timed out error page has
      // already been committed, or reload it if and when a timeout commits.
      SetState(STATE_NEEDS_RELOAD);
      return;

    case STATE_NEEDS_RELOAD:
    case STATE_NONE:
      // If the tab needs to reload or is in STATE_NONE, do nothing.  The reload
      // case shouldn't be very common, since it only lasts until a tab times
      // out, but it's still possible.
      return;

    default:
      NOTREACHED();
  }
}

void CaptivePortalTabReloader::OnSlowSSLConnect() {
  SetState(STATE_MAYBE_BROKEN_BY_PORTAL);
}

void CaptivePortalTabReloader::SetState(State new_state) {
  // Stop the timer even when old and new states are the same.
  if (state_ == STATE_TIMER_RUNNING) {
    slow_ssl_load_timer_.Stop();
  } else {
    DCHECK(!slow_ssl_load_timer_.IsRunning());
  }

  // Check for unexpected state transitions.
  switch (state_) {
    case STATE_NONE:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_TIMER_RUNNING);
      break;
    case STATE_TIMER_RUNNING:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_MAYBE_BROKEN_BY_PORTAL ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_MAYBE_BROKEN_BY_PORTAL:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_BROKEN_BY_PORTAL ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_BROKEN_BY_PORTAL:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_NEEDS_RELOAD:
      DCHECK_EQ(STATE_NONE, new_state);
      break;
    default:
      NOTREACHED();
      break;
  };

  state_ = new_state;

  switch (state_) {
    case STATE_TIMER_RUNNING:
      slow_ssl_load_timer_.Start(
          FROM_HERE,
          slow_ssl_load_time_,
          this,
          &CaptivePortalTabReloader::OnSlowSSLConnect);
      break;

    case STATE_MAYBE_BROKEN_BY_PORTAL:
      CheckForCaptivePortal();
      break;

    case STATE_NEEDS_RELOAD:
      // Try to reload the tab now.
      ReloadTabIfNeeded();
      break;

    default:
      break;
  }
}

void CaptivePortalTabReloader::ReloadTabIfNeeded() {
  // If there's still a provisional load going, or the page no longer needs
  // to be reloaded, due to a new navigation, do nothing.
  if (state_ != STATE_NEEDS_RELOAD || provisional_main_frame_load_)
    return;
  SetState(STATE_NONE);
  ReloadTab();
}

void CaptivePortalTabReloader::ReloadTab() {
  content::NavigationController* controller = &web_contents_->GetController();
  if (!controller->GetActiveEntry()->GetHasPostData())
    controller->Reload(true);
}

void CaptivePortalTabReloader::MaybeOpenCaptivePortalLoginTab() {
  open_login_tab_callback_.Run();
}

void CaptivePortalTabReloader::CheckForCaptivePortal() {
  CaptivePortalService* service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  if (service)
    service->DetectCaptivePortal();
}

}  // namespace captive_portal
