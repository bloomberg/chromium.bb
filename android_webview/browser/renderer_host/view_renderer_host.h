// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDER_HOST_VIEW_RENDERER_HOST_H_
#define ANDROID_WEBVIEW_BROWSER_RENDER_HOST_VIEW_RENDERER_HOST_H_

#include "base/threading/non_thread_safe.h"
#include "content/public/browser/web_contents_observer.h"

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class ViewRendererHost : public content::WebContentsObserver,
                         public base::NonThreadSafe {
 public:
  class Client {
   public:
    virtual void OnPictureUpdated(int process_id, int render_view_id) = 0;
    virtual void OnPageScaleFactorChanged(int process_id,
                                          int render_view_id,
                                          float page_scale_factor) = 0;

   protected:
    virtual ~Client() {}
  };

  ViewRendererHost(content::WebContents* contents, Client* client);
  virtual ~ViewRendererHost();

  // Captures the latest available picture pile synchronously.
  void CapturePictureSync();

  // Enables updating picture piles on every new frame.
  // OnPictureUpdated is called when a new picture is available,
  // stored by renderer id in RendererPictureMap.
  void EnableCapturePictureCallback(bool enabled);

  using content::WebContentsObserver::Observe;

 private:
  // content::WebContentsObserver implementation.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnPictureUpdated();
  void OnDidActivateAcceleratedCompositing(int input_handler_id);
  void OnPageScaleFactorChanged(float page_scale_factor);

  bool IsRenderViewReady() const;

  Client* client_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDER_HOST_VIEW_RENDERER_HOST_H_
