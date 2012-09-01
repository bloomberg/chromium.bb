// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_
#define ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_

#include "content/public/browser/web_contents_observer.h"

#include "base/threading/non_thread_safe.h"

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class AwRenderViewHostExt : public content::WebContentsObserver,
                            public base::NonThreadSafe {
 public:
  // To send receive messages to a RenderView we take the WebContents instance,
  // as it internally handles RenderViewHost instances changing underneath us.
  AwRenderViewHostExt(content::WebContents* contents);
  virtual ~AwRenderViewHostExt();

  // |result| will be invoked with the outcome of the request.
  typedef base::Callback<void(bool)> DocumentHasImagesResult;
  void DocumentHasImages(DocumentHasImagesResult result);

 private:
  // content::WebContentsObserver implementation.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDocumentHasImagesResponse(int msg_id, bool has_images);

  std::map<int, DocumentHasImagesResult> pending_document_has_images_requests_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderViewHostExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDER_HOST_RENDER_VIEW_HOST_EXT_H_
