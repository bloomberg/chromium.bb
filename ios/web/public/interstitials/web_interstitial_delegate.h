// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_DELEGATE_H_
#define IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_DELEGATE_H_

#include <string>

namespace web {

// Controls and provides the html for an interstitial page. The delegate is
// owned by the WebInterstitial.
class WebInterstitialDelegate {
 public:
  virtual ~WebInterstitialDelegate() {}

  // Returns the HTML that should be displayed in the page.
  virtual std::string GetHtmlContents() const = 0;

  // Called when the interstitial is proceeded or cancelled. Note that this may
  // be called directly even if the embedder didn't call Proceed or DontProceed
  // on WebInterstitial, since navigations etc may cancel them.
  virtual void OnProceed() {}
  virtual void OnDontProceed() {}

  // Invoked when a WebInterstitial receives a command via JavaScript.
  virtual void CommandReceived(const std::string& command) {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_DELEGATE_H_
