// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_

#include <map>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"

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

// Tests the bookmarked state of the page.
class DeclarativeContentIsBookmarkedPredicate {
 public:
  ~DeclarativeContentIsBookmarkedPredicate();

  bool IsIgnored() const;
  // Evaluate for URL bookmarked state.
  bool Evaluate(bool url_is_bookmarked) const;

  static scoped_ptr<DeclarativeContentIsBookmarkedPredicate> Create(
      const Extension* extension,
      const base::Value& value,
      std::string* error);

 private:
  DeclarativeContentIsBookmarkedPredicate(
      scoped_refptr<const Extension> extension,
      bool is_bookmarked);

  scoped_refptr<const Extension> extension_;
  bool is_bookmarked_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedPredicate);
};

// Supports tracking of URL matches across tab contents in a browser context,
// and querying for the matching condition sets.
class DeclarativeContentIsBookmarkedConditionTracker
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  DeclarativeContentIsBookmarkedConditionTracker(
      content::BrowserContext* context,
      DeclarativeContentConditionTrackerDelegate* delegate);
  ~DeclarativeContentIsBookmarkedConditionTracker() override;

  // Creates a new DeclarativeContentIsBookmarkedPredicate from |value|. Sets
  // *|error| and returns null if creation failed for any reason.
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error);

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

  // Returns true if |contents| current URL is bookmarked.
  bool IsUrlBookmarked(content::WebContents* contents) const;

 private:
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

    void BookmarkAddedForUrl(const GURL& url);
    void BookmarkRemovedForUrls(const std::set<GURL>& urls);
    void UpdateState(bool request_evaluation_if_unchanged);

    bool is_url_bookmarked() {
      return is_url_bookmarked_;
    }

   private:
    bool IsCurrentUrlBookmarked();

    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    bool is_url_bookmarked_;
    const RequestEvaluationCallback request_evaluation_;
    const WebContentsDestroyedCallback web_contents_destroyed_;

    DISALLOW_COPY_AND_ASSIGN(PerWebContentsTracker);
  };

  // bookmarks::BookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void ExtensiveBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void ExtensiveBookmarkChangesEnded(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(
      bookmarks::BookmarkModel* model) override;

  // Called by PerWebContentsTracker on web contents destruction.
  void DeletePerWebContentsTracker(content::WebContents* tracker);

  // Updates the bookmark state of all per-WebContents trackers.
  void UpdateAllPerWebContentsTrackers();

  // Maps WebContents to the tracker for that WebContents state.
  std::map<content::WebContents*,
           linked_ptr<PerWebContentsTracker>> per_web_contents_tracker_;

  // Count of the number of extensive bookmarks changes in progress (e.g. due to
  // sync). The rules need only be evaluated once after the extensive changes
  // are complete.
  int extensive_bookmark_changes_in_progress_;

  // Weak.
  DeclarativeContentConditionTrackerDelegate* delegate_;

  ScopedObserver<bookmarks::BookmarkModel, bookmarks::BookmarkModelObserver>
      scoped_bookmarks_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentIsBookmarkedConditionTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_IS_BOOKMARKED_CONDITION_TRACKER_H_
