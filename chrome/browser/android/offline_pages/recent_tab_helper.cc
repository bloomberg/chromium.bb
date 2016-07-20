// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::RecentTabHelper);

namespace {
class DefaultDelegate: public offline_pages::RecentTabHelper::Delegate {
 public:
  DefaultDelegate() {}

  // offline_pages::RecentTabHelper::Delegate
  std::unique_ptr<offline_pages::OfflinePageArchiver> CreatePageArchiver(
      content::WebContents* web_contents) override {
    return base::MakeUnique<offline_pages::OfflinePageMHTMLArchiver>(
        web_contents);
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override {
    return offline_pages::OfflinePageUtils::GetTabId(web_contents, tab_id);
  }
};
}  // namespace

namespace offline_pages {

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      page_model_(nullptr),
      delegate_(new DefaultDelegate()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RecentTabHelper::~RecentTabHelper() {
}

void RecentTabHelper::SetDelegate(
    std::unique_ptr<RecentTabHelper::Delegate> delegate) {
  DCHECK(delegate);
  delegate_ = std::move(delegate);
}

void RecentTabHelper::LazyInitialize() {
  snapshot_controller_.reset(new SnapshotController(delegate_->GetTaskRunner(),
                                                    this));
  int tab_id_number = 0;
  tab_id_.clear();

  if (delegate_->GetTabId(web_contents(), &tab_id_number))
    tab_id_ = base::IntToString(tab_id_number);

  page_model_ = OfflinePageModelFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  // TODO(dimich): When we have BackgroundOffliner, avoid capturing prerenderer
  // WebContents with its origin as well.
  never_do_snapshots_ = tab_id_.empty() ||
                        web_contents()->GetBrowserContext()->IsOffTheRecord();
}

void RecentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Initialize lazily. It needs TabAndroid for initization, which is also a
  // TabHelper - so can't initialize in constructor because of uncertain order
  // of creation of TabHelpers.
  if (!snapshot_controller_)
    LazyInitialize();

  if (navigation_handle->IsInMainFrame() &&
      navigation_handle->HasCommitted()) {
    // Cancel tasks in flight that relate to the previous page.
    weak_ptr_factory_.InvalidateWeakPtrs();

    // New navigation, new snapshot session.
    snapshot_url_ = GURL();
    GURL last_committed_url = web_contents()->GetLastCommittedURL();

    // Check for conditions that would cause us not to snapshot.
    bool can_save = !navigation_handle->IsErrorPage() &&
                    OfflinePageModel::CanSaveURL(last_committed_url);

    // We only want to record UMA if the page is not Off The Record.
    if (!never_do_snapshots_)
      UMA_HISTOGRAM_BOOLEAN("OfflinePages.CanSaveRecentPage", can_save);

    // Always reset so that posted tasks get cancelled.
    snapshot_controller_->Reset();

    if (never_do_snapshots_ || !can_save)
      snapshot_controller_->Stop();
    else
      snapshot_url_ = last_committed_url;
  }
}

void RecentTabHelper::DocumentAvailableInMainFrame() {
  snapshot_controller_->DocumentAvailableInMainFrame();
}

void RecentTabHelper::DocumentOnLoadCompletedInMainFrame() {
  // TODO(dimich): Figure out when this can fire before DidFinishNavigate().
  // See bug 628716 for more info.
  if (!snapshot_controller_)
    return;
  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
}

// This starts a sequence of async operations chained through callbacks:
// - compute the set of old 'last_n' pages that have to be purged
// - delete the pages found in the previous step
// - snapshot the current web contents
// Along the chain, the original URL is passed and compared, to detect
// possible navigation and cancel snapshot in that case.
void RecentTabHelper::StartSnapshot() {
  // Remove previously captured pages for this tab.
  page_model_->GetOfflineIdsForClientId(
    client_id(),
    base::Bind(&RecentTabHelper::ContinueSnapshotWithIdsToPurge,
               weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::ContinueSnapshotWithIdsToPurge(
    const std::vector<int64_t>& page_ids) {
  page_model_->DeletePagesByOfflineId(
      page_ids, base::Bind(&RecentTabHelper::ContinueSnapshotAfterPurge,
                           weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::ContinueSnapshotAfterPurge(
    OfflinePageModel::DeletePageResult result) {
  if (result != OfflinePageModel::DeletePageResult::SUCCESS) {
    // If previous pages can't be deleted, don't add new ones.
    ReportSnapshotCompleted();
    return;
  }

  if (!IsSamePage()) {
    ReportSnapshotCompleted();
    return;
  }

  page_model_->SavePage(
      snapshot_url_, client_id(),
      delegate_->CreatePageArchiver(web_contents()),
      base::Bind(&RecentTabHelper::SavePageCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::SavePageCallback(OfflinePageModel::SavePageResult result,
                                       int64_t offline_id) {
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
  return ClientId(kLastNNamespace, tab_id_);
}

}  // namespace offline_pages
