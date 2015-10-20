// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/page_revisit_broadcaster.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/sync/glue/synced_session_util.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/revisit/bookmarks_by_url_provider_impl.h"
#include "components/sync_sessions/revisit/bookmarks_page_revisit_observer.h"
#include "components/sync_sessions/revisit/sessions_page_revisit_observer.h"
#include "components/sync_sessions/revisit/typed_url_page_revisit_observer.h"

namespace browser_sync {

namespace {

// Simple implementation of ForeignSessionsProvider that delegates to
// SessionsSyncManager. It holds onto a non-owning pointer, with the assumption
// that this class is only used by classes owned by SessionsSyncManager itself.
class SessionsSyncManagerWrapper
    : public sync_sessions::ForeignSessionsProvider {
 public:
  explicit SessionsSyncManagerWrapper(SessionsSyncManager* manager)
      : manager_(manager) {}
  ~SessionsSyncManagerWrapper() override{};
  bool GetAllForeignSessions(
      std::vector<const sync_driver::SyncedSession*>* sessions) override {
    return manager_->GetAllForeignSessions(sessions);
  }

 private:
  SessionsSyncManager* manager_;
  DISALLOW_COPY_AND_ASSIGN(SessionsSyncManagerWrapper);
};

}  // namespace

PageRevisitBroadcaster::PageRevisitBroadcaster(
    SessionsSyncManager* sessions,
    history::HistoryService* history,
    bookmarks::BookmarkModel* bookmarks) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("PageRevisitInstrumentation");
  bool shouldInstrument = group_name == "Enabled";
  if (shouldInstrument) {
    revisit_observers_.push_back(new sync_sessions::SessionsPageRevisitObserver(
        scoped_ptr<sync_sessions::ForeignSessionsProvider>(
            new SessionsSyncManagerWrapper(sessions))));

    revisit_observers_.push_back(
        new sync_sessions::TypedUrlPageRevisitObserver(history));

    revisit_observers_.push_back(
        new sync_sessions::BookmarksPageRevisitObserver(
            scoped_ptr<sync_sessions::BookmarksByUrlProvider>(
                new sync_sessions::BookmarksByUrlProviderImpl(bookmarks))));
  }
}

PageRevisitBroadcaster::~PageRevisitBroadcaster() {}

void PageRevisitBroadcaster::OnPageVisit(const GURL& url,
                                         const ui::PageTransition transition) {
  if (ShouldSyncURL(url)) {
    sync_sessions::PageVisitObserver::TransitionType converted(
        ConvertTransitionEnum(transition));
    for (auto* observer : revisit_observers_) {
      observer->OnPageVisit(url, converted);
    }
  }
}

// Static
sync_sessions::PageVisitObserver::TransitionType
PageRevisitBroadcaster::ConvertTransitionEnum(
    const ui::PageTransition original) {
  switch (ui::PageTransitionStripQualifier(original)) {
    case ui::PAGE_TRANSITION_LINK:
      if (original & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
        return sync_sessions::PageVisitObserver::kTransitionCopyPaste;
      } else {
        return sync_sessions::PageVisitObserver::kTransitionPage;
      }
    case ui::PAGE_TRANSITION_TYPED:
      return sync_sessions::PageVisitObserver::kTransitionOmniboxUrl;

    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return sync_sessions::PageVisitObserver::kTransitionBookmark;

    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      // These are not expected, we only expect top-level frame transitions.
      return sync_sessions::PageVisitObserver::kTransitionUnknown;

    case ui::PAGE_TRANSITION_GENERATED:
      return sync_sessions::PageVisitObserver::kTransitionOmniboxDefaultSearch;

    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      if (original & ui::PAGE_TRANSITION_FORWARD_BACK) {
        return sync_sessions::PageVisitObserver::kTransitionForwardBackward;
      } else {
        return sync_sessions::PageVisitObserver::kTransitionUnknown;
      }

    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return sync_sessions::PageVisitObserver::kTransitionPage;

    case ui::PAGE_TRANSITION_RELOAD:
      // Refreshing pages also carry PAGE_TRANSITION_RELOAD but the url never
      // changes so we don't expect to ever get them.
      return sync_sessions::PageVisitObserver::kTransitionRestore;

    case ui::PAGE_TRANSITION_KEYWORD:
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return sync_sessions::PageVisitObserver::kTransitionOmniboxTemplateSearch;

    default:
      return sync_sessions::PageVisitObserver::kTransitionUnknown;
  }
}

}  // namespace browser_sync
