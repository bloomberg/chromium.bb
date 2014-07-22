// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prefetch_helper.h"

#include "chrome/common/prefetch_messages.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebURLLoaderOptions.h"

namespace prefetch {

PrefetchHelper::PrefetchHelper(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

PrefetchHelper::~PrefetchHelper() {
  STLDeleteElements(&loader_set_);
}

bool PrefetchHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrefetchHelper, message)
    IPC_MESSAGE_HANDLER(PrefetchMsg_Prefetch, OnPrefetch)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrefetchHelper::OnPrefetch(const GURL& url) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  blink::WebURLRequest request(url);
  request.setRequestContext(blink::WebURLRequest::RequestContextPrefetch);
  request.setPriority(blink::WebURLRequest::PriorityVeryLow);
  blink::WebURLLoaderOptions options;
  options.allowCredentials = true;
  options.crossOriginRequestPolicy =
      blink::WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
  blink::WebURLLoader* loader = frame->createAssociatedURLLoader(options);
  loader->loadAsynchronously(request, this);
  loader_set_.insert(loader);
}

void PrefetchHelper::didFinishLoading(blink::WebURLLoader* loader,
                                      double finishTime,
                                      int64_t totalEncodedDataLength) {
  loader_set_.erase(loader);
}

void PrefetchHelper::didFail(blink::WebURLLoader* loader,
                             const blink::WebURLError& error) {
  loader_set_.erase(loader);
}

}  // namespace prefetch
