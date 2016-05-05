// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_

#include "chrome/browser/android/offline_pages/prerendering_loader.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/offline_page_model.h"

class PrerenderManager;

namespace content {
class WebContents;
}  // namespace content

namespace offline_pages {

class OfflinerPolicy;

// An Offliner implementation that attempts client-side rendering and saving
// of an offline page. It uses the PrerenderingLoader to load the page and
// the OfflinePageModel to save it.
class PrerenderingOffliner : public Offliner {
 public:
  PrerenderingOffliner(const OfflinerPolicy* policy,
                       PrerenderManager* prerender_manager,
                       OfflinePageModel* offline_page_model);
  ~PrerenderingOffliner() override;

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override;

  void Cancel() override;

 private:
  std::unique_ptr<PrerenderingLoader> loader_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_OFFLINER_H_
