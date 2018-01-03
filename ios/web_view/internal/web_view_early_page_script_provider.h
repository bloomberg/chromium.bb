// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_EARLY_PAGE_SCRIPT_PROVIDER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_EARLY_PAGE_SCRIPT_PROVIDER_H_

#include <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace web {
class BrowserState;
}

namespace ios_web_view {

// A provider class associated with a single web::BrowserState object. Keeps
// an early page script, a JavaScript injected into all pages as early as
// possible.
//
// Not threadsafe. Must be used only on the main thread.
class WebViewEarlyPageScriptProvider : public base::SupportsUserData::Data {
 public:
  ~WebViewEarlyPageScriptProvider() override;

  // Returns a provider for the given |browser_state|. Lazily attaches one if it
  // does not exist. |browser_state| can not be null.
  static WebViewEarlyPageScriptProvider& FromBrowserState(
      web::BrowserState* _Nonnull browser_state);

  // Getter and Setter for the JavaScript source code.
  NSString* _Nonnull GetScript() { return script_; }
  void SetScript(NSString* _Nonnull script);

 private:
  WebViewEarlyPageScriptProvider();

  // The JavaScript source code.
  NSString* _Nonnull script_;

  DISALLOW_COPY_AND_ASSIGN(WebViewEarlyPageScriptProvider);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_EARLY_PAGE_SCRIPT_PROVIDER_H_
