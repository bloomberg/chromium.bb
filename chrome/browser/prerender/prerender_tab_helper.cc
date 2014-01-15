// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_local_predictor.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
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
    PasswordManager* password_manager) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              new PrerenderTabHelper(web_contents,
                                                     password_manager));
  }
}

PrerenderTabHelper::PrerenderTabHelper(content::WebContents* web_contents,
                                       PasswordManager* password_manager)
    : content::WebContentsObserver(web_contents),
      weak_factory_(this) {
  password_manager->AddSubmissionCallback(
      base::Bind(&PrerenderTabHelper::PasswordSubmitted,
                 weak_factory_.GetWeakPtr()));
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
  prerender_manager->MarkWebContentsAsNotPrerendered(web_contents());
  ReportTabHelperURLSeenToLocalPredictor(prerender_manager, url,
                                         web_contents());
}

void PrerenderTabHelper::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
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
  // Compute the PPLT metric and report it in a histogram, if needed.
  // We include pages that are still prerendering and have just finished
  // loading -- PrerenderManager will sort this out and handle it correctly
  // (putting those times into a separate histogram).
  if (!pplt_load_start_.is_null()) {
    double fraction_elapsed_at_swapin = -1.0;
    base::TimeTicks now = base::TimeTicks::Now();
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
    PrerenderManager::RecordPerceivedPageLoadTime(
        now - pplt_load_start_, fraction_elapsed_at_swapin, web_contents(),
        url_);
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
  actual_load_start_ = base::TimeTicks();
}

void PrerenderTabHelper::DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) {
  if (is_main_frame) {
    // Record the beginning of a new PPLT navigation.
    pplt_load_start_ = base::TimeTicks::Now();
    actual_load_start_ = base::TimeTicks();
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

bool PrerenderTabHelper::IsPrerendered() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendered(web_contents(), NULL);
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  // Ensure we are not prerendering any more.
  DCHECK(!IsPrerendering());
  if (pplt_load_start_.is_null()) {
    // If we have already finished loading, report a 0 PPLT.
    PrerenderManager::RecordPerceivedPageLoadTime(base::TimeDelta(), 1.0,
                                                  web_contents(), url_);
  } else {
    // If we have not finished loading yet, record the actual load start, and
    // rebase the start time to now.
    actual_load_start_ = pplt_load_start_;
    pplt_load_start_ = base::TimeTicks::Now();
  }
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

}  // namespace prerender
