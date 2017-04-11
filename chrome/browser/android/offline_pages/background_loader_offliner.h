// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/content/background_loader/background_loader_contents.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/snapshot_controller.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace offline_pages {

class OfflinerPolicy;
class OfflinePageModel;

// An Offliner implementation that attempts client-side rendering and saving
// of an offline page. It uses the BackgroundLoader to load the page and the
// OfflinePageModel to save it. Only one request may be active at a time.
class BackgroundLoaderOffliner : public Offliner,
                                 public content::WebContentsObserver,
                                 public SnapshotController::Client {
 public:
  BackgroundLoaderOffliner(content::BrowserContext* browser_context,
                           const OfflinerPolicy* policy,
                           OfflinePageModel* offline_page_model);
  ~BackgroundLoaderOffliner() override;

  static BackgroundLoaderOffliner* FromWebContents(
      content::WebContents* contents);

  // Offliner implementation.
  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& completion_callback,
                   const ProgressCallback& progress_callback) override;
  void Cancel(const CancelCallback& callback) override;
  bool HandleTimeout(const SavePageRequest& request) override;

  // WebContentsObserver implementation.
  void DocumentAvailableInMainFrame() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // SnapshotController::Client implementation.
  void StartSnapshot() override;

  void SetPageDelayForTest(long delay_ms);
  void OnNetworkBytesChanged(int64_t bytes);

 protected:
  // Called to reset internal loader and observer state.
  virtual void ResetState();

 private:
  friend class TestBackgroundLoaderOffliner;

  enum SaveState { NONE, SAVING, DELETE_AFTER_SAVE };
  enum PageLoadState { SUCCESS, RETRIABLE, NONRETRIABLE, DELAY_RETRY };

  // Called when the page has been saved.
  void OnPageSaved(SavePageResult save_result, int64_t offline_id);

  // Called when application state has changed.
  void OnApplicationStateChange(
      base::android::ApplicationState application_state);
  void HandleApplicationStateChangeCancel(const SavePageRequest& request,
                                          int64_t offline_id);

  std::unique_ptr<background_loader::BackgroundLoaderContents> loader_;
  // Not owned.
  content::BrowserContext* browser_context_;
  // Not owned.
  OfflinePageModel* offline_page_model_;
  // Tracks pending request, if any.
  std::unique_ptr<SavePageRequest> pending_request_;
  // Handles determining when a page should be snapshotted.
  std::unique_ptr<SnapshotController> snapshot_controller_;
  // Callback when pending request completes.
  CompletionCallback completion_callback_;
  // Callback to report progress.
  ProgressCallback progress_callback_;
  // ApplicationStatusListener to monitor if Chrome moves to the foreground.
  std::unique_ptr<base::android::ApplicationStatusListener> app_listener_;
  // Whether we are on a low-end device.
  bool is_low_end_device_;

  // Save state.
  SaveState save_state_;
  // Page load state.
  PageLoadState page_load_state_;
  // Seconds to delay before taking snapshot.
  long page_delay_ms_;
  // Network bytes loaded.
  int64_t network_bytes_;

  // Callback for cancel.
  CancelCallback cancel_callback_;

  base::WeakPtrFactory<BackgroundLoaderOffliner> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundLoaderOffliner);
};

}  // namespace offline_pages
#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_BACKGROUND_LOADER_OFFLINER_H_
