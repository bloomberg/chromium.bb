// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_VIEW_HANDLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_VIEW_HANDLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class Profile;

namespace views {
class WebView;
}

namespace chromeos {

// Owns a views::WebView instance. Any caller actively using the views::WebView
// instance should increase/decrease the refcount.
class WebViewHandle : public base::RefCounted<WebViewHandle> {
 public:
  explicit WebViewHandle(Profile* profile);

  // Returns an unowned pointer to the stored |web_view| instance.
  views::WebView* web_view() { return web_view_.get(); }

 private:
  friend class base::RefCounted<WebViewHandle>;
  ~WebViewHandle();

  std::unique_ptr<views::WebView> web_view_;

  DISALLOW_COPY_AND_ASSIGN(WebViewHandle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEB_VIEW_HANDLE_H_
