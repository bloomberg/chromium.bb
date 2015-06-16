// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

//
// PerWebContentsTracker
//

DeclarativeContentPageUrlConditionTracker::PerWebContentsTracker::
PerWebContentsTracker(
    content::WebContents* contents,
    url_matcher::URLMatcher* url_matcher,
    const RequestEvaluationCallback& request_evaluation,
    const WebContentsDestroyedCallback& web_contents_destroyed)
    : WebContentsObserver(contents),
      url_matcher_(url_matcher),
      request_evaluation_(request_evaluation),
      web_contents_destroyed_(web_contents_destroyed) {
}

DeclarativeContentPageUrlConditionTracker::PerWebContentsTracker::
~PerWebContentsTracker() {
}

void DeclarativeContentPageUrlConditionTracker::PerWebContentsTracker::
UpdateMatchesForCurrentUrl() {
  matches_ = url_matcher_->MatchURL(web_contents()->GetURL());
  request_evaluation_.Run(web_contents());
}

void DeclarativeContentPageUrlConditionTracker::PerWebContentsTracker::
WebContentsDestroyed() {
  web_contents_destroyed_.Run(web_contents());
}

//
// DeclarativeContentPageUrlConditionTracker
//

DeclarativeContentPageUrlConditionTracker::
DeclarativeContentPageUrlConditionTracker(
    content::BrowserContext* context,
    DeclarativeContentConditionTrackerDelegate* delegate)
    : context_(context),
      delegate_(delegate) {
}

DeclarativeContentPageUrlConditionTracker::
~DeclarativeContentPageUrlConditionTracker() {
}

void DeclarativeContentPageUrlConditionTracker::AddConditionSets(
    const url_matcher::URLMatcherConditionSet::Vector& condition_sets) {
  url_matcher_.AddConditionSets(condition_sets);
  UpdateMatchesForAllTrackers();
}

void DeclarativeContentPageUrlConditionTracker::RemoveConditionSets(
    const std::vector<url_matcher::URLMatcherConditionSet::ID>&
    condition_set_ids) {
  url_matcher_.RemoveConditionSets(condition_set_ids);
  UpdateMatchesForAllTrackers();
}

void DeclarativeContentPageUrlConditionTracker::ClearUnusedConditionSets() {
  url_matcher_.ClearUnusedConditionSets();
}

void DeclarativeContentPageUrlConditionTracker::TrackForWebContents(
    content::WebContents* contents) {
  per_web_contents_tracker_[contents] =
      make_linked_ptr(new PerWebContentsTracker(
          contents,
          &url_matcher_,
          base::Bind(&DeclarativeContentConditionTrackerDelegate::
                     RequestEvaluation,
                     base::Unretained(delegate_)),
          base::Bind(&DeclarativeContentPageUrlConditionTracker::
                     DeletePerWebContentsTracker,
                     base::Unretained(this))));
  per_web_contents_tracker_[contents]->UpdateMatchesForCurrentUrl();
}

void DeclarativeContentPageUrlConditionTracker::OnWebContentsNavigation(
    content::WebContents* contents,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  per_web_contents_tracker_[contents]->UpdateMatchesForCurrentUrl();
}

void DeclarativeContentPageUrlConditionTracker::GetMatches(
    content::WebContents* contents,
    std::set<url_matcher::URLMatcherConditionSet::ID>* matches) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  *matches = per_web_contents_tracker_[contents]->matches();
}

void DeclarativeContentPageUrlConditionTracker::DeletePerWebContentsTracker(
    content::WebContents* contents) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  per_web_contents_tracker_.erase(contents);
}

bool DeclarativeContentPageUrlConditionTracker::IsEmpty() const {
  return url_matcher_.IsEmpty();
}

void DeclarativeContentPageUrlConditionTracker::UpdateMatchesForAllTrackers() {
  for (const auto& web_contents_tracker_pair : per_web_contents_tracker_)
    web_contents_tracker_pair.second->UpdateMatchesForCurrentUrl();
}

}  // namespace extensions
