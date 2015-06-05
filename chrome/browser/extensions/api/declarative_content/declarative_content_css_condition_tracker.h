// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"


namespace content {
class BrowserContext;
struct FrameNavigateParams;
struct LoadCommittedDetails;
class RenderProcessHost;
class WebContents;
}

namespace extensions {

// Interface that allows the DeclarativeContentCssConditionTracker to request
// condition evaluation, and shields it from having to know about Browsers.
class DeclarativeContentCssConditionTrackerDelegate {
 public:
  // Callback for per-tab invocations.
  using WebContentsCallback =
      base::Callback<void(content::WebContents* content)>;

  // Requests re-evaluation of conditions for |contents|.
  virtual void RequestEvaluation(content::WebContents* contents) = 0;

  // Returns true if the evaluator should manage condition state for |context|.
  virtual bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) = 0;

 protected:
  DeclarativeContentCssConditionTrackerDelegate();
  virtual ~DeclarativeContentCssConditionTrackerDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTrackerDelegate);
};

// Supports watching of CSS selectors to across tab contents in a browser
// context, and querying for the matching CSS selectors for a context.
class DeclarativeContentCssConditionTracker
    : public content::NotificationObserver {
 public:
  DeclarativeContentCssConditionTracker(
      content::BrowserContext* context,
      DeclarativeContentCssConditionTrackerDelegate* delegate);
  ~DeclarativeContentCssConditionTracker() override;

  // Sets the set of CSS selectors to watch for CSS condition evaluation.
  void SetWatchedCssSelectors(
      const std::set<std::string>& watched_css_selectors);

  // Requests that CSS conditions be tracked for |contents|.
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

  // Inserts the currently-matching CSS selectors into |css_selectors|. We use
  // hash_set for maximally efficient lookup.
  void GetMatchingCssSelectors(content::WebContents* contents,
                               base::hash_set<std::string>* css_selectors);

  // TODO(wittman): Remove once DeclarativeChromeContentRulesRegistry no longer
  // depends on concrete condition implementations. At that point
  // DeclarativeChromeContentRulesRegistryTest.ActiveRulesDoesntGrow will be
  // able to use a test condition object and not need to depend on force setting
  // matching CSS seleectors.
  void UpdateMatchingCssSelectorsForTesting(
      content::WebContents* contents,
      const std::vector<std::string>& matching_css_selectors);

 private:
  class PerWebContentsTracker;

  // Instantiate a PerWebContentsTracker watching |contents|.
  void CreatePerWebContentsTracker(content::WebContents* contents);

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // If the renderer process is associated with our browser context, tells it
  // what page attributes to watch for using an ExtensionMsg_WatchPages.
  void InstructRenderProcessIfManagingBrowserContext(
      content::RenderProcessHost* process);

  // Called by PerWebContentsTracker on destruction.
  void RemovePerWebContentsTracker(PerWebContentsTracker* tracker);

  // The context whose state we're monitoring for evaluation.
  content::BrowserContext* context_;

  // All CSS selectors that are watched for by any rule's conditions. This
  // vector is sorted by construction.
  std::vector<std::string> watched_css_selectors_;

  // Maps WebContents to the tracker for that WebContents
  // state. PerWebContentsTracker owns itself, so this pointer is weak.
  std::map<content::WebContents*, PerWebContentsTracker*>
      per_web_contents_tracker_;

  // Weak.
  DeclarativeContentCssConditionTrackerDelegate* delegate_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_
