// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_
#define CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_

#include "content/public/browser/web_contents_observer.h"

namespace chrome {

// A WebViewGuest is a WebContentsObserver on the guest WebContents of a
// <webview> tag. It provides the browser-side implementation of the <webview>
// API and manages the lifetime of <webview> extension events.
class WebViewGuest : public content::WebContentsObserver {
 public:
  WebViewGuest(content::WebContents* guest_web_contents,
               content::WebContents* embedder_web_contents,
               const std::string& extension_id);

  static WebViewGuest* From(void* profile, int instance_id);

  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  content::WebContents* web_contents() const {
    return WebContentsObserver::web_contents();
  }

 private:
  virtual ~WebViewGuest();
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  content::WebContents* embedder_web_contents_;
  const std::string extension_id_;
  const int embedder_render_process_id_;
  // Profile and instance ID are cached here because |web_contents()| is
  // null on destruction.
  void* profile_;
  const int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_WEBVIEW_WEBVIEW_GUEST_H_
