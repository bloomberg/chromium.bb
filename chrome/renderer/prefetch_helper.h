// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PREFETCH_HELPER_H_
#define CHROME_RENDERER_PREFETCH_HELPER_H_

#include <set>

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "url/gurl.h"

namespace prefetch {

// Helper class initiating prefetches on behalf of a RenderFrame.
class PrefetchHelper : public content::RenderFrameObserver,
                       public blink::WebURLLoaderClient {
 public:
  explicit PrefetchHelper(content::RenderFrame* render_frame);
  virtual ~PrefetchHelper();

  // blink::WebURLLoaderClient implementation
  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finishTime,
                                int64_t totalEncodedDataLength) OVERRIDE;
  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error) OVERRIDE;

 private:
  // RenderViewObserver implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnPrefetch(const GURL& url);

  std::set<blink::WebURLLoader*> loader_set_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchHelper);
};

}  // namespace prefetch

#endif  // CHROME_RENDERER_PREFETCH_HELPER_H_
