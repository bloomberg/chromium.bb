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
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
struct FrameNavigateParams;
struct LoadCommittedDetails;
class RenderProcessHost;
class WebContents;
}

namespace extensions {

class Extension;

// Tests whether all the specified CSS selectors match on the page.
class DeclarativeContentCssPredicate {
 public:
  ~DeclarativeContentCssPredicate();

  const std::vector<std::string>& css_selectors() const {
    return css_selectors_;
  }

  static scoped_ptr<DeclarativeContentCssPredicate> Create(
      const base::Value& value,
      std::string* error);

 private:
  explicit DeclarativeContentCssPredicate(
      const std::vector<std::string>& css_selectors);
  std::vector<std::string> css_selectors_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssPredicate);
};

class DeclarativeContentConditionTrackerDelegate;

// Supports watching of CSS selectors to across tab contents in a browser
// context, and querying for the matching CSS selectors for a context.
class DeclarativeContentCssConditionTracker
    : public content::NotificationObserver {
 public:
  DeclarativeContentCssConditionTracker(
      content::BrowserContext* context,
      DeclarativeContentConditionTrackerDelegate* delegate);
  ~DeclarativeContentCssConditionTracker() override;

  // Creates a new DeclarativeContentCssPredicate from |value|. Sets
  // *|error| and returns null if creation failed for any reason.
  scoped_ptr<DeclarativeContentCssPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error);

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

  // Returns the result of evaluating |predicate| on the per-tab state
  // associated with |contents|.
  bool EvaluatePredicate(const DeclarativeContentCssPredicate* predicate,
                         content::WebContents* contents) const;

  // TODO(wittman): Remove once DeclarativeChromeContentRulesRegistry no longer
  // depends on concrete condition implementations. At that point
  // DeclarativeChromeContentRulesRegistryTest.ActiveRulesDoesntGrow will be
  // able to use a test condition object and not need to depend on force setting
  // matching CSS seleectors.
  void UpdateMatchingCssSelectorsForTesting(
      content::WebContents* contents,
      const std::vector<std::string>& matching_css_selectors);

 private:
  // Monitors CSS selector matching state on one WebContents.
  class PerWebContentsTracker : public content::WebContentsObserver {
   public:
    using RequestEvaluationCallback =
        base::Callback<void(content::WebContents*)>;
    using WebContentsDestroyedCallback =
        base::Callback<void(content::WebContents*)>;

    PerWebContentsTracker(
        content::WebContents* contents,
        const RequestEvaluationCallback& request_evaluation,
        const WebContentsDestroyedCallback& web_contents_destroyed);
    ~PerWebContentsTracker() override;

    void OnWebContentsNavigation(const content::LoadCommittedDetails& details,
                                 const content::FrameNavigateParams& params);

    // See comment on similar function above.
    void UpdateMatchingCssSelectorsForTesting(
        const std::vector<std::string>& matching_css_selectors);

    const base::hash_set<std::string>& matching_css_selectors() const {
      return matching_css_selectors_;
    }

   private:
    // content::WebContentsObserver overrides.
    bool OnMessageReceived(const IPC::Message& message) override;
    void WebContentsDestroyed() override;

    void OnWatchedPageChange(const std::vector<std::string>& css_selectors);

    const RequestEvaluationCallback request_evaluation_;
    const WebContentsDestroyedCallback web_contents_destroyed_;

    // We use a hash_set for maximally efficient lookup.
    base::hash_set<std::string> matching_css_selectors_;

    DISALLOW_COPY_AND_ASSIGN(PerWebContentsTracker);
  };

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // If the renderer process is associated with our browser context, tells it
  // what page attributes to watch for using an ExtensionMsg_WatchPages.
  void InstructRenderProcessIfManagingBrowserContext(
      content::RenderProcessHost* process);

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* contents);

  // The context whose state we're monitoring for evaluation.
  content::BrowserContext* context_;

  // All CSS selectors that are watched for by any rule's conditions. This
  // vector is sorted by construction.
  std::vector<std::string> watched_css_selectors_;

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*, linked_ptr<PerWebContentsTracker>>
      per_web_contents_tracker_;

  // Weak.
  DeclarativeContentConditionTrackerDelegate* delegate_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentCssConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CSS_CONDITION_TRACKER_H_
