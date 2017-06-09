// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/page_revisit_broadcaster.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/revisit/bookmarks_by_url_provider_impl.h"
#include "components/sync_sessions/revisit/bookmarks_page_revisit_observer.h"
#include "components/sync_sessions/revisit/sessions_page_revisit_observer.h"
#include "components/sync_sessions/revisit/typed_url_page_revisit_observer.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {

namespace {

// Simple implementation of ForeignSessionsProvider that delegates to
// SessionsSyncManager. It holds onto a non-owning pointer, with the assumption
// that this class is only used by classes owned by SessionsSyncManager itself.
class SessionsSyncManagerWrapper : public ForeignSessionsProvider {
 public:
  explicit SessionsSyncManagerWrapper(SessionsSyncManager* manager)
      : manager_(manager) {}
  ~SessionsSyncManagerWrapper() override {}
  bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) override {
    return manager_->GetAllForeignSessions(sessions);
  }

 private:
  SessionsSyncManager* manager_;
  DISALLOW_COPY_AND_ASSIGN(SessionsSyncManagerWrapper);
};

}  // namespace

PageRevisitBroadcaster::PageRevisitBroadcaster(
    SessionsSyncManager* manager,
    SyncSessionsClient* sessions_client)
    : sessions_client_(sessions_client) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("PageRevisitInstrumentation");
  bool shouldInstrument = group_name == "Enabled";
  if (shouldInstrument) {
    revisit_observers_.push_back(base::MakeUnique<SessionsPageRevisitObserver>(
        base::MakeUnique<SessionsSyncManagerWrapper>(manager)));

    history::HistoryService* history = sessions_client_->GetHistoryService();
    if (history) {
      revisit_observers_.push_back(
          base::MakeUnique<TypedUrlPageRevisitObserver>(history));
    }

    bookmarks::BookmarkModel* bookmarks = sessions_client_->GetBookmarkModel();
    if (bookmarks) {
      revisit_observers_.push_back(
          base::MakeUnique<BookmarksPageRevisitObserver>(
              base::MakeUnique<BookmarksByUrlProviderImpl>(bookmarks)));
    }
  }
}

PageRevisitBroadcaster::~PageRevisitBroadcaster() {}

void PageRevisitBroadcaster::OnPageVisit(const GURL& url,
                                         const ui::PageTransition transition) {
  if (sessions_client_->ShouldSyncURL(url)) {
    PageVisitObserver::TransitionType converted(
        ConvertTransitionEnum(transition));
    for (auto& observer : revisit_observers_) {
      observer->OnPageVisit(url, converted);
    }
  }
}

// Static
PageVisitObserver::TransitionType PageRevisitBroadcaster::ConvertTransitionEnum(
    const ui::PageTransition original) {
  switch (ui::PageTransitionStripQualifier(original)) {
    case ui::PAGE_TRANSITION_LINK:
      if (original & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
        return PageVisitObserver::kTransitionCopyPaste;
      } else {
        return PageVisitObserver::kTransitionPage;
      }
    case ui::PAGE_TRANSITION_TYPED:
      return PageVisitObserver::kTransitionOmniboxUrl;

    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return PageVisitObserver::kTransitionBookmark;

    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      // These are not expected, we only expect top-level frame transitions.
      return PageVisitObserver::kTransitionUnknown;

    case ui::PAGE_TRANSITION_GENERATED:
      return PageVisitObserver::kTransitionOmniboxDefaultSearch;

    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      if (original & ui::PAGE_TRANSITION_FORWARD_BACK) {
        return PageVisitObserver::kTransitionForwardBackward;
      } else {
        return PageVisitObserver::kTransitionUnknown;
      }

    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return PageVisitObserver::kTransitionPage;

    case ui::PAGE_TRANSITION_RELOAD:
      // Refreshing pages also carry PAGE_TRANSITION_RELOAD but the url never
      // changes so we don't expect to ever get them.
      return PageVisitObserver::kTransitionRestore;

    case ui::PAGE_TRANSITION_KEYWORD:
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return PageVisitObserver::kTransitionOmniboxTemplateSearch;

    default:
      return PageVisitObserver::kTransitionUnknown;
  }
}

}  // namespace sync_sessions
