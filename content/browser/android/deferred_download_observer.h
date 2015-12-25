// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DEFERRED_DOWNLOAD_OBSERVER_H_
#define CONTENT_BROWSER_ANDROID_DEFERRED_DOWNLOAD_OBSERVER_H_

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;

// Class for handling deferred downloads inside a WebContents. In document
// mode, it is possible that a download starts before ContentViewCore gets
// created. This class listens to the WebContentsObserver::WasShown() method
// before running the download tasks to make sure ContentViewCore is attached
// to WebContents.
// TODO(qinmin): Remove this when java Tab object creation is no longer pending
// on Activity creation.
class DeferredDownloadObserver : public WebContentsObserver {
 public:
  DeferredDownloadObserver(WebContents* web_contents,
                           const base::Closure& deferred_download_cb);
  ~DeferredDownloadObserver() override;

  // WebContentsObserver method.
  void WasShown() override;
  void WebContentsDestroyed() override;

 private:
  // Callback to run when WebContents gets shown.
  base::Closure deferred_download_cb_;

  DISALLOW_COPY_AND_ASSIGN(DeferredDownloadObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DEFERRED_DOWNLOAD_OBSERVER_H_
