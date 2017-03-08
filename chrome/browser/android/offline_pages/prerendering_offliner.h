// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace offline_pages {

class OfflinerPolicy;

// An Offliner implementation that attempts client-side rendering and saving
// of an offline page. It uses the PrerenderingLoader to load the page and
// the OfflinePageModel to save it. Only one request may be active at a time.
class PrerenderingOffliner : public Offliner {
 public:
  PrerenderingOffliner(content::BrowserContext* browser_context,
                       const OfflinerPolicy* policy,
                       OfflinePageModel* offline_page_model);
  ~PrerenderingOffliner() override;

  // Offliner implementation.
  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& completion_callback,
                   const ProgressCallback& progress_callback) override;
  void Cancel(const CancelCallback& callback) override;
  bool HandleTimeout(const SavePageRequest& request) override;

  // Allows a loader to be injected for testing. This may only be done once
  // and must be called before any of the Offliner interface methods are called.
  void SetLoaderForTesting(std::unique_ptr<PrerenderingLoader> loader);

  void SetLowEndDeviceForTesting(bool is_low_end_device);

  void SetApplicationStateForTesting(
      base::android::ApplicationState application_state);

 protected:
  // Internal method for requesting OfflinePageModel to save page.  Exposed for
  // unit testing.
  // TODO(dougarnett): Consider making OfflinePageModel mockable instead.
  virtual void SavePage(
      const OfflinePageModel::SavePageParams& save_page_params,
      std::unique_ptr<OfflinePageArchiver> archiver,
      const SavePageCallback& save_callback);

 private:
  // Progress callback for PrerenderingLoader::LoadPage().
  void OnNetworkProgress(const SavePageRequest& request, int64_t bytes);

  // Callback logic for PrerenderingLoader::LoadPage().
  void OnLoadPageDone(const SavePageRequest& request,
                      Offliner::RequestStatus load_status,
                      content::WebContents* web_contents);

  // Callback logic for OfflinePageModel::SavePage().
  void OnSavePageDone(const SavePageRequest& request,
                      SavePageResult save_result,
                      int64_t offline_id);

  PrerenderingLoader* GetOrCreateLoader();

  // Listener function for changes to application background/foreground state.
  void OnApplicationStateChange(
      base::android::ApplicationState application_state);
  void HandleApplicationStateChangeCancel(const SavePageRequest& request,
                                          int64_t offline_id);

  // Not owned.
  content::BrowserContext* browser_context_;
  // Not owned.
  const OfflinerPolicy* policy_;
  // Not owned.
  OfflinePageModel* offline_page_model_;
  // Lazily created.
  std::unique_ptr<PrerenderingLoader> loader_;
  // Tracks pending request, if any. Owned copy.
  // May be used to ensure a callback applies to the pending request (e.g., in
  // case we receive a save page callback for an old, canceled request).
  std::unique_ptr<SavePageRequest> pending_request_;
  // Callback to call when pending request completes/fails.
  CompletionCallback completion_callback_;
  ProgressCallback progress_callback_;
  bool is_low_end_device_;
  // ApplicationStatusListener to monitor if the Chrome moves to the foreground.
  std::unique_ptr<base::android::ApplicationStatusListener> app_listener_;
  base::WeakPtrFactory<PrerenderingOffliner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOffliner);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
