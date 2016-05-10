// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_loader.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace offline_pages {

PrerenderingLoader::PrerenderingLoader(
    content::BrowserContext* browser_context) {}

PrerenderingLoader::~PrerenderingLoader() {}

bool PrerenderingLoader::LoadPage(
    const GURL& url,
    const LoadPageCallback& callback) {
  // TODO(dougarnett): implement.
  return false;
}

void PrerenderingLoader::StopLoading() {
  // TODO(dougarnett): implement.
}

}  // namespace offline_pages
