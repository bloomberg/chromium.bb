// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_VIEW_COUNTER_H_
#define IOS_WEB_PUBLIC_WEB_VIEW_COUNTER_H_

#include <stddef.h>

#include "base/macros.h"

namespace web {

class BrowserState;

// Maintains the count of web views associated with a particular BrowserState.
// Must be used only from the UI thread.
class WebViewCounter {
 public:
  // Returns the web::WebViewCounter for the given |browser_state|. Lazily
  // attaches one if it does not exist. |browser_state| cannot be null.
  static web::WebViewCounter* FromBrowserState(
      web::BrowserState* browser_state);

  // Returns the count of WKWebViews associated with a BrowserState.
  virtual size_t GetWKWebViewCount() = 0;

 protected:
  WebViewCounter() {};
  virtual ~WebViewCounter() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewCounter);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_VIEW_COUNTER_H_
