// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/downloads/download_ui_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/snapshot_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}

namespace offline_pages {

using PageQuality = SnapshotController::PageQuality;

// Attaches to every WebContent shown in a tab. Waits until the WebContent is
// loaded to proper degree and then makes a snapshot of the page. Removes the
// oldest snapshot in the 'ring buffer'. As a result, there is always up to N
// snapshots of recent pages on the device.
class RecentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RecentTabHelper>,
      public SnapshotController::Client {
 public:
  ~RecentTabHelper() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void WebContentsDestroyed() override;
  void WasHidden() override;

  // SnapshotController::Client
  void StartSnapshot() override;

  // Delegate that is used by RecentTabHelper to get external dependencies.
  // Default implementation lives in .cc file, while tests provide an override.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OfflinePageArchiver> CreatePageArchiver(
        content::WebContents* web_contents) = 0;
    virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;
    // There is no expectations that tab_id is always present.
    virtual bool GetTabId(content::WebContents* web_contents, int* tab_id) = 0;
  };
  void SetDelegate(std::unique_ptr<RecentTabHelper::Delegate> delegate);

  // Support for Download Page feature. The Download Page button does this:
  // 1. Creates suspended request for Background Offliner.
  // 2. Calls this method with properly filled ClientId.
  // 3. This tab helper observes the page load and captures the page
  //    if the load progresses far enough, as indicated by SnapshotController.
  // 4. If capture is successful, this tab helper removes the suspended request.
  //    Otherwise (navigation to other page, close tab) tab helper un-suspends
  //    the request so the Background Offliner starts working on it.
  // 5. If Chrome is killed at any point, next time Background Offliner loads
  //    it converts all suspended requests from last session into active.
  void ObserveAndDownloadCurrentPage(const ClientId& client_id,
                                     int64_t request_id);

 private:
  // Keeps client_id/request_id that will be used for the offline snapshot.
  struct SnapshotProgressInfo {
   public:
    // For a downloads snapshot request, where the |request_id| is defined.
    SnapshotProgressInfo(const ClientId& client_id, int64_t request_id)
        : client_id(client_id), request_id(request_id) {}

    // For a last_n snapshot request.
    explicit SnapshotProgressInfo(const ClientId& client_id)
        : client_id(client_id) {}

    bool IsForLastN();

    // The ClientID to go with the offline page.
    ClientId client_id;

    // Id of the suspended request in Background Offliner. Used to un-suspend
    // the request if the capture of the current page was not possible (e.g.
    // the user navigated to another page before current one was loaded).
    // 0 if this is a "last_n" info.
    int64_t request_id = OfflinePageModel::kInvalidOfflineId;

    // True if there was at least one snapshot successfully completed.
    bool page_snapshot_completed = false;

    // Expected snapshot quality should the saving succeed. This value is only
    // valid if |page_snapshot_completed| is true.
    PageQuality expected_page_quality = PageQuality::POOR;
  };

  explicit RecentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<RecentTabHelper>;

  bool EnsureInitialized();
  void ContinueSnapshotWithIdsToPurge(SnapshotProgressInfo* snapshot_info,
                                      const std::vector<int64_t>& page_ids);
  void ContinueSnapshotAfterPurge(SnapshotProgressInfo* snapshot_info,
                                  OfflinePageModel::DeletePageResult result);
  void SavePageCallback(SnapshotProgressInfo* snapshot_info,
                        OfflinePageModel::SavePageResult result,
                        int64_t offline_id);
  void ReportSnapshotCompleted(SnapshotProgressInfo* snapshot_info);
  void ReportDownloadStatusToRequestCoordinator(
      SnapshotProgressInfo* snapshot_info);
  ClientId GetRecentPagesClientId() const;
  void SaveSnapshotForDownloads();

  // Page model is a service, no ownership. Can be null - for example, in
  // case when tab is in incognito profile.
  OfflinePageModel* page_model_ = nullptr;

  // If false, never make snapshots off the attached WebContents.
  // Not page-specific.
  bool snapshots_enabled_ = false;

  // Snapshot progress information for a downloads triggered request. Null if
  // downloads is not capturing or hasn't captured the current page.
  std::unique_ptr<SnapshotProgressInfo> downloads_latest_snapshot_info_;

  // Snapshot progress information for a last_n triggered request. Null if
  // last_n is not currently capturing the current page.
  std::unique_ptr<SnapshotProgressInfo> last_n_ongoing_snapshot_info_;

  // If empty, the tab does not have AndroidId and can not capture pages.
  std::string tab_id_;

  // The URL of the page that is currently being snapshotted. Used to check,
  // during async operations, that WebContents still contains the same page.
  GURL snapshot_url_;

  // Monitors page loads and starts snapshots when a download request exist. It
  // is also used as an initialization flag for EnsureInitialized() to be run
  // only once.
  std::unique_ptr<SnapshotController> snapshot_controller_;

  std::unique_ptr<Delegate> delegate_;

  // Set at each navigation to control if last_n should save snapshots of the
  // current page being loaded.
  bool last_n_listen_to_tab_hidden_ = false;

  // Track the page quality status of the last saved snapshot for the current
  // page. It is generally reset upon new navigations.
  PageQuality last_n_latest_saved_quality_ = PageQuality::POOR;

  base::WeakPtrFactory<RecentTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelper);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
