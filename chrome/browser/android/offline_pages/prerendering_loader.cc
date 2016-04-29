// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_loader.h"

#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace offline_pages {

PrerenderingLoader::PrerenderingLoader(PrerenderManager* prerender_manager) {}

PrerenderingLoader::~PrerenderingLoader() {}

bool PrerenderingLoader::LoadPage(
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size,
    const LoadPageCallback& callback) {
  // TODO(dougarnett): implement.
  return false;
}

void PrerenderingLoader::StopLoading() {
  // TODO(dougarnett): implement.
}

}  // namespace offline_pages
