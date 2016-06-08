// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_types.h"

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
                   const CompletionCallback& callback) override;
  void Cancel() override;

  // Allows a loader to be injected for testing. This may only be done once
  // and must be called before any of the Offliner interface methods are called.
  void SetLoaderForTesting(std::unique_ptr<PrerenderingLoader> loader);

 protected:
  // Internal method for requesting OfflinePageModel to save page.
  // Exposed for unit testing.
  // TODO(dougarnett): Consider making OfflinePageModel mockable instead.
  virtual void SavePage(const GURL& url,
                        const ClientId& client_id,
                        std::unique_ptr<OfflinePageArchiver> archiver,
                        const SavePageCallback& callback);

 private:
  // Callback logic for PrerenderingLoader::LoadPage().
  void OnLoadPageDone(const SavePageRequest& request,
                      const CompletionCallback& completion_callback,
                      Offliner::RequestStatus load_status,
                      content::WebContents* web_contents);

  // Callback logic for OfflinePageModel::SavePage().
  void OnSavePageDone(const SavePageRequest& request,
                      const CompletionCallback& completion_callback,
                      SavePageResult save_result,
                      int64_t offline_id);

  PrerenderingLoader* GetOrCreateLoader();
  void SetPendingRequest(int64_t request_id);
  void ClearPendingRequest();

  // Not owned.
  content::BrowserContext* browser_context_;
  // Not owned.
  OfflinePageModel* offline_page_model_;
  // Lazily created.
  std::unique_ptr<PrerenderingLoader> loader_;
  // Tracks pending request, if any. Owned copy.
  // May be used to ensure a callback applies to the pending request (e.g., in
  // case we receive a save page callback for an old, canceled request).
  std::unique_ptr<SavePageRequest> pending_request_;
  base::WeakPtrFactory<PrerenderingOffliner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOffliner);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
