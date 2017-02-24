// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerender_adapter.h"

#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace offline_pages {

PrerenderAdapter::Observer::Observer() {}
PrerenderAdapter::Observer::~Observer() {}

PrerenderAdapter::PrerenderAdapter(PrerenderAdapter::Observer* observer)
    : observer_(observer) {
  DCHECK(observer);
}

PrerenderAdapter::~PrerenderAdapter() {
  if (IsActive())
    DestroyActive();
}

bool PrerenderAdapter::StartPrerender(
    content::BrowserContext* browser_context,
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  DCHECK(!IsActive());

  prerender::PrerenderManager* manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(browser_context);
  DCHECK(manager);

  // AddPrerenderForOffline() triggers the start event, and it is before the
  // observer is set.
  active_handle_ =
      manager->AddPrerenderForOffline(url, session_storage_namespace, size);
  if (!active_handle_)
    return false;
  DCHECK(active_handle_->contents());
  DCHECK(active_handle_->contents()->prerendering_has_started());

  active_handle_->SetObserver(this);

  return true;
}

content::WebContents* PrerenderAdapter::GetWebContents() const {
  DCHECK(IsActive());
  DCHECK(active_handle_->contents());
  // Note: the prerender stack maintains ownership of these contents
  // and PrerenderingLoader::StopLoading() must be called to report
  // the Loader is done with the contents.
  return active_handle_->contents()->prerender_contents();
}

prerender::FinalStatus PrerenderAdapter::GetFinalStatus() const {
  DCHECK(IsActive());
  DCHECK(active_handle_->contents());
  return active_handle_->contents()->final_status();
}

bool PrerenderAdapter::IsActive() const {
  return active_handle_.get();
}

void PrerenderAdapter::DestroyActive() {
  DCHECK(IsActive());
  // Clear active handle and call OnCancel from local variable to avoid
  // immediate Observer OnPrerenderStop() call from thinking still active.
  prerender::PrerenderHandle* handle_to_destroy = active_handle_.release();
  handle_to_destroy->OnCancel();
  delete handle_to_destroy;
}

void PrerenderAdapter::OnPrerenderStart(prerender::PrerenderHandle* handle) {
  // Not expected as the start event will happen before the observer is set.
  NOTREACHED();
}

void PrerenderAdapter::OnPrerenderStopLoading(
    prerender::PrerenderHandle* handle) {
  DCHECK_EQ(active_handle_.get(), handle);
  observer_->OnPrerenderStopLoading();
}

void PrerenderAdapter::OnPrerenderDomContentLoaded(
    prerender::PrerenderHandle* handle) {
  DCHECK_EQ(active_handle_.get(), handle);
  observer_->OnPrerenderDomContentLoaded();
}

void PrerenderAdapter::OnPrerenderStop(prerender::PrerenderHandle* handle) {
  if (IsActive()) {
    DCHECK_EQ(active_handle_.get(), handle);
    observer_->OnPrerenderStop();
  }
}

void PrerenderAdapter::OnPrerenderNetworkBytesChanged(
    prerender::PrerenderHandle* handle) {
  if (IsActive()) {
    DCHECK_EQ(active_handle_.get(), handle);
    prerender::PrerenderContents* contents = handle->contents();
    if (!contents)
      return;
    observer_->OnPrerenderNetworkBytesChanged(contents->network_bytes());
  }
}

}  // namespace offline_pages
