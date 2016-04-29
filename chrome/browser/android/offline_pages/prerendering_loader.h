// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_

#include "base/callback.h"
#include "components/offline_pages/background/offliner.h"

class GURL;
class PrerenderManager;

namespace content {
class WebContents;
class SessionStorageNamespace;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace offline_pages {

// A client-side page loader that integrates with the PrerenderManager to do
// the page loading in the background.
class PrerenderingLoader {
 public:
  // Reports status of a load page request with loaded contents if available.
  typedef base::Callback<void(Offliner::CompletionStatus,
                              content::WebContents*)>
      LoadPageCallback;

  explicit PrerenderingLoader(PrerenderManager* prerender_manager);
  ~PrerenderingLoader();

  // Loads a page in the background if possible and returns whether the
  // request was accepted. If so, the LoadPageCallback will be informed
  // of status. Only one load request may exist as a time. If a previous
  // request is still in progress it must be canceled before a new
  // request will be accepted.
  bool LoadPage(const GURL& url,
                content::SessionStorageNamespace* session_storage_namespace,
                const gfx::Size& size,
                const LoadPageCallback& callback);

  // Stops (completes or cancels) the load request. Must be called when
  // LoadPageCallback is done with consuming the contents.
  // This loader should also be responsible for stopping offline
  // prerenders when Chrome is transitioned to foreground.
  void StopLoading();
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_PRERENDERING_LOADER_H_
