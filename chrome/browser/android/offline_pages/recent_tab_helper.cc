// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::RecentTabHelper);

namespace {
const char* kClientNamespace = "last_n";

// Max number of pages to keep. The oldest pages that are over this count are
// deleted before the next one is saved.
const size_t kMaxPagesToKeep = 50;

// Predicate for priority_queue used to compute the oldest pages.
struct ComparePagesForPurge {
  bool operator()(const offline_pages::OfflinePageItem* left,
                 const offline_pages::OfflinePageItem* right) const {
    return left->creation_time > right->creation_time;
  }
};

}  // namespace

namespace offline_pages {

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      page_model_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  snapshot_controller_.reset(new SnapshotController(
      base::ThreadTaskRunnerHandle::Get(), this));
  page_model_ = OfflinePageModelFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  // TODO(dimich): When we have BackgroundOffliner, avoid capturing prerenderer
  // WebContents with its origin as well.
  never_do_snapshots_ = web_contents->GetBrowserContext()->IsOffTheRecord();
}

RecentTabHelper::~RecentTabHelper() {
}

void RecentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      navigation_handle->HasCommitted() &&
      !navigation_handle->IsErrorPage()) {
    // New navigation, new snapshot session.
    snapshot_controller_->Reset();
    snapshot_url_ = GURL::EmptyGURL();
  }
}

void RecentTabHelper::DocumentAvailableInMainFrame() {
  snapshot_controller_->DocumentAvailableInMainFrame();
}

void RecentTabHelper::DocumentOnLoadCompletedInMainFrame() {
  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
}

// This starts a sequence of async operations chained through callbacks:
// - compute the set of old 'last_n' pages that have to be purged
// - delete the pages found in the previous step
// - snapshot the current web contents
// Along the chain, the original URL is passed and compared, to detect
// possible navigation and cancel snapshot in that case.
void RecentTabHelper::StartSnapshot() {
  if (never_do_snapshots_)
    return;

  GURL url = web_contents()->GetLastCommittedURL();
  if (!page_model_->CanSavePage(url)) {
    // TODO(dimich): Add UMA. Bug 608112.
    return;
  }

  snapshot_url_ = url;

  // TODO(dimich): Implement automatic cleanup as per design doc, based on
  // storage limits and page age.
  // This algorithm (remove pages before making sure the save was successful)
  // prefers keeping the storage use low and under control by potentially
  // sacrificing the current snapshot.
  page_model_->GetOfflineIdsForClientId(
    client_id(),
    base::Bind(&RecentTabHelper::ContinueSnapshotWithIdsToPurge,
               weak_ptr_factory_.GetWeakPtr()));
}

// Collects following pages from lastN namespace:
// - the ones with the same online URL
// - the oldest pages, enough to keep kMaxPagesToKeep limit.
void RecentTabHelper::ContinueSnapshotWithIdsToPurge(
    const std::vector<int64_t>& page_ids) {

  std::vector<int64_t> pages_to_purge;
  // Use priority queue to figure out the set of oldest pages to purge.
  std::priority_queue<const OfflinePageItem*,
                      std::vector<const OfflinePageItem*>,
                      ComparePagesForPurge> pages_queue;

  for (const auto& offline_id : page_ids) {
    // TODO(dimich): get rid of 'Maybe'. Maybe make it return multiple pages.
    const OfflinePageItem* page =
        page_model_->MaybeGetPageByOfflineId(offline_id);
    // If there is already a snapshot of this page, remove it so we don't
    // have multiple snapshots of the same page.
    if (page->url == snapshot_url_) {
      pages_to_purge.push_back(offline_id);
    } else {
      pages_queue.push(page);
    }
  }

  // Negative counter means nothing else to purge.
  int count_to_purge =
      page_ids.size() - kMaxPagesToKeep - pages_to_purge.size();

  for (int count = 0; count < count_to_purge; ++count) {
    pages_to_purge.push_back(pages_queue.top()->offline_id);
    pages_queue.pop();
  }

  page_model_->DeletePagesByOfflineId(
      pages_to_purge, base::Bind(&RecentTabHelper::ContinueSnapshotAfterPurge,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::ContinueSnapshotAfterPurge(
    OfflinePageModel::DeletePageResult result) {
  // NOT_FOUND is because it's what we get when passing empty vector of ids.
  // TODO(dimich): remove NOT_FOUND when bug 608057 is fixed.
  if (result != OfflinePageModel::DeletePageResult::SUCCESS &&
      result != OfflinePageModel::DeletePageResult::NOT_FOUND) {
    // If previous pages can't be deleted, don't add new ones.
    ReportSnapshotCompleted();
    return;
  }

  if (!IsSamePage()) {
    ReportSnapshotCompleted();
    return;
  }

  // Create either test Archiver or a regular one.
  std::unique_ptr<OfflinePageArchiver> archiver;
  if (test_archive_factory_.get()) {
    archiver = test_archive_factory_->CreateArchiver(web_contents());
  } else {
    archiver.reset(new OfflinePageMHTMLArchiver(web_contents()));
  }

  page_model_->SavePage(
      snapshot_url_, client_id(), std::move(archiver),
      base::Bind(&RecentTabHelper::SavePageCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::SavePageCallback(OfflinePageModel::SavePageResult result,
                                       int64_t offline_id) {
  // TODO(dimich): add UMA, including result. Bug 608112.
  ReportSnapshotCompleted();
}

void RecentTabHelper::ReportSnapshotCompleted() {
  snapshot_controller_->PendingSnapshotCompleted();
}

bool RecentTabHelper::IsSamePage() const {
  return web_contents() &&
         (web_contents()->GetLastCommittedURL() == snapshot_url_);
}

ClientId RecentTabHelper::client_id() const {
  return ClientId(kClientNamespace, "");
}

void RecentTabHelper::SetArchiveFactoryForTest(
    std::unique_ptr<TestArchiveFactory> test_archive_factory) {
  test_archive_factory_ = std::move(test_archive_factory);
}

void RecentTabHelper::SetTaskRunnerForTest(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  snapshot_controller_.reset(new SnapshotController(task_runner, this));
}

}  // namespace offline_pages
