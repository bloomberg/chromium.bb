// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

#include "base/bind.h"
#include "components/offline_pages/background/save_page_request.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

PrerenderingOffliner::PrerenderingOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      weak_ptr_factory_(this) {}

PrerenderingOffliner::~PrerenderingOffliner() {}

void PrerenderingOffliner::OnLoadPageDone(
    const Offliner::CompletionStatus load_status,
    content::WebContents* contents) {
  // TODO(dougarnett): Implement save attempt and running CompletionCallback.
}

bool PrerenderingOffliner::LoadAndSave(const SavePageRequest& request,
                                       const CompletionCallback& callback) {
  if (!offline_page_model_->CanSavePage(request.url()))
    return false;

  // Kick off load page attempt.
  return GetOrCreateLoader()->LoadPage(
      request.url(), base::Bind(&PrerenderingOffliner::OnLoadPageDone,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PrerenderingOffliner::Cancel() {
  GetOrCreateLoader()->StopLoading();
}

PrerenderingLoader* PrerenderingOffliner::GetOrCreateLoader() {
  if (!loader_) {
    loader_.reset(new PrerenderingLoader(browser_context_));
  }
  return loader_.get();
}

void PrerenderingOffliner::SetLoaderForTesting(
    std::unique_ptr<PrerenderingLoader> loader) {
  DCHECK(!loader_);
  loader_ = std::move(loader);
}

}  // namespace offline_pages
