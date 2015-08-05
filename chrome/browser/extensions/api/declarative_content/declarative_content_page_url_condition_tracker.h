// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
struct FrameNavigateParams;
struct LoadCommittedDetails;
class WebContents;
}

namespace extensions {

class Extension;

// Tests the URL of a page against conditions specified with the
// URLMatcherConditionSet.
class DeclarativeContentPageUrlPredicate {
 public:
  ~DeclarativeContentPageUrlPredicate();

  url_matcher::URLMatcherConditionSet* url_matcher_condition_set() const {
    return url_matcher_condition_set_.get();
  }

  // Evaluate for match IDs for the URL of the top-level page of the renderer.
  bool Evaluate(
      const std::set<url_matcher::URLMatcherConditionSet::ID>&
          page_url_matches) const;

  static scoped_ptr<DeclarativeContentPageUrlPredicate> Create(
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& value,
      std::string* error);

 private:
  explicit DeclarativeContentPageUrlPredicate(
      scoped_refptr<url_matcher::URLMatcherConditionSet>
          url_matcher_condition_set);

  scoped_refptr<url_matcher::URLMatcherConditionSet> url_matcher_condition_set_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentPageUrlPredicate);
};

// Supports tracking of URL matches across tab contents in a browser context,
// and querying for the matching condition sets.
class DeclarativeContentPageUrlConditionTracker {
 public:
  using PredicateFactory =
      base::Callback<scoped_ptr<DeclarativeContentPageUrlPredicate>(
          const base::Value& value,
          std::string* error)>;

  DeclarativeContentPageUrlConditionTracker(
      content::BrowserContext* context,
      DeclarativeContentConditionTrackerDelegate* delegate);
  ~DeclarativeContentPageUrlConditionTracker();

  // Creates a new DeclarativeContentPageUrlPredicate from |value|. Sets
  // *|error| and returns null if creation failed for any reason.
  scoped_ptr<DeclarativeContentPageUrlPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error);

  // Adds new URLMatcherConditionSet to the URL Matcher. Each condition set
  // must have a unique ID.
  void AddConditionSets(
      const url_matcher::URLMatcherConditionSet::Vector& condition_sets);

  // Removes the listed condition sets. All |condition_set_ids| must be
  // currently registered.
  void RemoveConditionSets(
      const std::vector<url_matcher::URLMatcherConditionSet::ID>&
      condition_set_ids);

  // Removes all unused condition sets from the ConditionFactory.
  void ClearUnusedConditionSets();

  // Returns the URLMatcherConditionFactory that must be used to create
  // URLMatcherConditionSets for this tracker.
  url_matcher::URLMatcherConditionFactory* condition_factory() {
    return url_matcher_.condition_factory();
  }

  // Requests that URL matches be tracked for |contents|.
  void TrackForWebContents(content::WebContents* contents);

  // Handles navigation of |contents|. We depend on the caller to notify us of
  // this event rather than listening for it ourselves, so that the caller can
  // coordinate evaluation with all the trackers that respond to it. If we
  // listened ourselves and requested rule evaluation before another tracker
  // received the notification, our conditions would be evaluated based on the
  // new URL while the other tracker's conditions would still be evaluated based
  // on the previous URL.
  void OnWebContentsNavigation(content::WebContents* contents,
                               const content::LoadCommittedDetails& details,
                               const content::FrameNavigateParams& params);

  // Gets the maching IDs for the last navigation on WebContents.
  void GetMatches(content::WebContents* contents,
                  std::set<url_matcher::URLMatcherConditionSet::ID>* matches);

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  class PerWebContentsTracker : public content::WebContentsObserver {
   public:
    using RequestEvaluationCallback =
        base::Callback<void(content::WebContents*)>;
    using WebContentsDestroyedCallback =
        base::Callback<void(content::WebContents*)>;

    PerWebContentsTracker(
        content::WebContents* contents,
        url_matcher::URLMatcher* url_matcher,
        const RequestEvaluationCallback& request_evaluation,
        const WebContentsDestroyedCallback& web_contents_destroyed);
    ~PerWebContentsTracker() override;

    void UpdateMatchesForCurrentUrl(bool request_evaluation_if_unchanged);

    const std::set<url_matcher::URLMatcherConditionSet::ID>& matches() {
      return matches_;
    }

   private:
    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    url_matcher::URLMatcher* url_matcher_;
    const RequestEvaluationCallback request_evaluation_;
    const WebContentsDestroyedCallback web_contents_destroyed_;

    std::set<url_matcher::URLMatcherConditionSet::ID> matches_;

    DISALLOW_COPY_AND_ASSIGN(PerWebContentsTracker);
  };

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* tracker);

  void UpdateMatchesForAllTrackers();

  // Matches URLs for all WebContents.
  url_matcher::URLMatcher url_matcher_;

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*,
           linked_ptr<PerWebContentsTracker>> per_web_contents_tracker_;

  // The context whose state we're monitoring for evaluation.
  content::BrowserContext* context_;

  // Weak.
  DeclarativeContentConditionTrackerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentPageUrlConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_PAGE_URL_CONDITION_TRACKER_H_
