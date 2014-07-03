// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_local_predictor.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(prerender::PrerenderTabHelper);

namespace prerender {

namespace {

void ReportTabHelperURLSeenToLocalPredictor(
    PrerenderManager* prerender_manager,
    const GURL& url,
    WebContents* web_contents) {
  if (!prerender_manager)
    return;
  PrerenderLocalPredictor* local_predictor =
      prerender_manager->local_predictor();
  if (!local_predictor)
    return;
  local_predictor->OnTabHelperURLSeen(url, web_contents);
}

}  // namespace

// static
void PrerenderTabHelper::CreateForWebContentsWithPasswordManager(
    content::WebContents* web_contents,
    password_manager::PasswordManager* password_manager) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              new PrerenderTabHelper(web_contents,
                                                     password_manager));
  }
}

PrerenderTabHelper::PrerenderTabHelper(
    content::WebContents* web_contents,
    password_manager::PasswordManager* password_manager)
    : content::WebContentsObserver(web_contents),
      origin_(ORIGIN_NONE),
      next_load_is_control_prerender_(false),
      next_load_origin_(ORIGIN_NONE),
      weak_factory_(this) {
  if (password_manager) {
    // May be NULL in testing.
    password_manager->AddSubmissionCallback(
        base::Bind(&PrerenderTabHelper::PasswordSubmitted,
                   weak_factory_.GetWeakPtr()));
  }

  // Determine if this is a prerender.
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents, &origin_)) {
    navigation_type_ = NAVIGATION_TYPE_PRERENDERED;
  } else {
    navigation_type_ = NAVIGATION_TYPE_NORMAL;
  }
}

PrerenderTabHelper::~PrerenderTabHelper() {
}

void PrerenderTabHelper::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    content::RenderFrameHost* render_frame_host) {
  url_ = url;
  RecordEvent(EVENT_MAINFRAME_CHANGE);
  RecordEventIfLoggedInURL(EVENT_MAINFRAME_CHANGE_DOMAIN_LOGGED_IN, url);
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  ReportTabHelperURLSeenToLocalPredictor(prerender_manager, url,
                                         web_contents());
}

void PrerenderTabHelper::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    content::PageTransition transition_type) {
  if (render_frame_host->GetParent())
    return;
  RecordEvent(EVENT_MAINFRAME_COMMIT);
  RecordEventIfLoggedInURL(EVENT_MAINFRAME_COMMIT_DOMAIN_LOGGED_IN,
                           validated_url);
  url_ = validated_url;
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  prerender_manager->RecordNavigation(validated_url);
  ReportTabHelperURLSeenToLocalPredictor(prerender_manager, validated_url,
                                         web_contents());
}

void PrerenderTabHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  // Compute the PPLT metric and report it in a histogram, if needed. If the
  // page is still prerendering, record the not swapped in page load time
  // instead.
  if (!pplt_load_start_.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (IsPrerendering()) {
      PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
      if (prerender_manager) {
        prerender_manager->RecordPageLoadTimeNotSwappedIn(
            origin_, now - pplt_load_start_, url_);
      } else {
        NOTREACHED();
      }
    } else {
      double fraction_elapsed_at_swapin = -1.0;
      if (!actual_load_start_.is_null()) {
        double plt = (now - actual_load_start_).InMillisecondsF();
        if (plt > 0.0) {
          fraction_elapsed_at_swapin = 1.0 -
              (now - pplt_load_start_).InMillisecondsF() / plt;
        } else {
          fraction_elapsed_at_swapin = 1.0;
        }
        DCHECK_GE(fraction_elapsed_at_swapin, 0.0);
        DCHECK_LE(fraction_elapsed_at_swapin, 1.0);
      }

      RecordPerceivedPageLoadTime(
          now - pplt_load_start_, fraction_elapsed_at_swapin);
    }
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
  actual_load_start_ = base::TimeTicks();
}

void PrerenderTabHelper::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (render_frame_host->GetParent())
    return;

  // Record PPLT state for the beginning of a new navigation.
  pplt_load_start_ = base::TimeTicks::Now();
  actual_load_start_ = base::TimeTicks();

  if (next_load_is_control_prerender_) {
    DCHECK_EQ(NAVIGATION_TYPE_NORMAL, navigation_type_);
    navigation_type_ = NAVIGATION_TYPE_WOULD_HAVE_BEEN_PRERENDERED;
    origin_ = next_load_origin_;
    next_load_is_control_prerender_ = false;
    next_load_origin_ = ORIGIN_NONE;
  }
}

void PrerenderTabHelper::PasswordSubmitted(const autofill::PasswordForm& form) {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager) {
    prerender_manager->RecordLikelyLoginOnURL(form.origin);
    RecordEvent(EVENT_LOGIN_ACTION_ADDED);
    if (form.password_value.empty())
      RecordEvent(EVENT_LOGIN_ACTION_ADDED_PW_EMPTY);
  }
}

PrerenderManager* PrerenderTabHelper::MaybeGetPrerenderManager() const {
  return PrerenderManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool PrerenderTabHelper::IsPrerendering() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendering(web_contents(), NULL);
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  // Ensure we are not prerendering any more.
  DCHECK_EQ(NAVIGATION_TYPE_PRERENDERED, navigation_type_);
  DCHECK(!IsPrerendering());
  if (pplt_load_start_.is_null()) {
    // If we have already finished loading, report a 0 PPLT.
    RecordPerceivedPageLoadTime(base::TimeDelta(), 1.0);
    DCHECK_EQ(NAVIGATION_TYPE_NORMAL, navigation_type_);
  } else {
    // If we have not finished loading yet, record the actual load start, and
    // rebase the start time to now.
    actual_load_start_ = pplt_load_start_;
    pplt_load_start_ = base::TimeTicks::Now();
  }
}

void PrerenderTabHelper::WouldHavePrerenderedNextLoad(Origin origin) {
  next_load_is_control_prerender_ = true;
  next_load_origin_ = origin;
}

void PrerenderTabHelper::RecordEvent(PrerenderTabHelper::Event event) const {
  UMA_HISTOGRAM_ENUMERATION("Prerender.TabHelperEvent",
                            event, PrerenderTabHelper::EVENT_MAX_VALUE);
}

void PrerenderTabHelper::RecordEventIfLoggedInURL(
    PrerenderTabHelper::Event event, const GURL& url) {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  scoped_ptr<bool> is_present(new bool);
  scoped_ptr<bool> lookup_succeeded(new bool);
  bool* is_present_ptr = is_present.get();
  bool* lookup_succeeded_ptr = lookup_succeeded.get();
  prerender_manager->CheckIfLikelyLoggedInOnURL(
      url,
      is_present_ptr,
      lookup_succeeded_ptr,
      base::Bind(&PrerenderTabHelper::RecordEventIfLoggedInURLResult,
                 weak_factory_.GetWeakPtr(),
                 event,
                 base::Passed(&is_present),
                 base::Passed(&lookup_succeeded)));
}

void PrerenderTabHelper::RecordEventIfLoggedInURLResult(
    PrerenderTabHelper::Event event,
    scoped_ptr<bool> is_present,
    scoped_ptr<bool> lookup_succeeded) {
  if (*lookup_succeeded && *is_present)
    RecordEvent(event);
}

void PrerenderTabHelper::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in) {
  DCHECK(!IsPrerendering());
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;

  // Note: it is possible for |next_load_is_control_prerender_| to be true at
  // this point. This does not affect the classification of the current load,
  // but only the next load. (This occurs if a WOULD_HAVE_BEEN_PRERENDERED
  // navigation interrupts and aborts another navigation.)
  prerender_manager->RecordPerceivedPageLoadTime(
      origin_, navigation_type_, perceived_page_load_time,
      fraction_plt_elapsed_at_swap_in, url_);

  // Reset state for the next navigation.
  navigation_type_ = NAVIGATION_TYPE_NORMAL;
  origin_ = ORIGIN_NONE;
}

}  // namespace prerender
