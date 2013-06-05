// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_
#define CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_

#include "content/public/browser/web_contents_observer.h"

namespace chrome {

// A WebViewGuest is a WebContentsObserver on the guest WebContents of a
// <webview> tag. It provides the browser-side implementation of the <webview>
// API and manages the lifetime of <webview> extension events. WebViewGuest is
// created on attachment. That is, when a guest WebContents is associated with
// a particular embedder WebContents. This happens on either initial navigation
// or through the use of the New Window API, when a new window is attached to
// a particular <webview>.
class WebViewGuest : public content::WebContentsObserver {
 public:
  WebViewGuest(content::WebContents* guest_web_contents,
               content::WebContents* embedder_web_contents,
               const std::string& extension_id,
               int webview_instance_id);

  static WebViewGuest* From(void* profile, int instance_id);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  content::WebContents* web_contents() const {
    return WebContentsObserver::web_contents();
  }

  int instance_id() const { return webview_instance_id_; }

 private:
  virtual ~WebViewGuest();
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  void AddWebViewToExtensionRendererState();
  static void RemoveWebViewFromExtensionRendererState(
      content::WebContents* web_contents);

  content::WebContents* embedder_web_contents_;
  const std::string extension_id_;
  const int embedder_render_process_id_;
  // Profile and instance ID are cached here because |web_contents()| is
  // null on destruction.
  void* profile_;
  // |guest_instance_id_| is a profile-wide unique identifier for a guest
  // WebContents.
  const int guest_instance_id_;
  // |webview_instance_id_| is an identifier that's unique within a particular
  // embedder RenderView for a particular <webview> instance.
  int webview_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_
