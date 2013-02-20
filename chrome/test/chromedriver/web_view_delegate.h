// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_WEB_VIEW_DELEGATE_H_
#define CHROME_TEST_CHROMEDRIVER_WEB_VIEW_DELEGATE_H_

class WebView;

class WebViewDelegate {
 public:
  virtual ~WebViewDelegate() {}

  // Listen to close event of a WebView.
  virtual void OnWebViewClose(WebView* web_view) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_WEB_VIEW_DELEGATE_H_
