// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_H_
#define IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_H_

#include <memory>

class GURL;

namespace web {

class HtmlWebInterstitialDelegate;
class NativeWebInterstitialDelegate;
class WebState;

// This class is used for showing interstitial pages, pages that show some
// informative message asking for user validation before reaching the target
// page. (Navigating to a page served over bad HTTPS or a page containing
// malware are typical cases where an interstitial is required.)
//
// WebInterstitial instances take care of deleting themselves when closed by the
// WebState or through a navigation.
class WebInterstitial {
 public:
  // Creates an interstitial page to show in |web_state|. Takes ownership of
  // |delegate|. Reloading the interstitial page will result in a new navigation
  // to |url|.  The pointers returned by these functions are self-owning; they
  // manage their own deletion after calling |Show()|.
  static WebInterstitial* CreateHtmlInterstitial(
      WebState* web_state,
      bool new_navigation,
      const GURL& url,
      std::unique_ptr<HtmlWebInterstitialDelegate> delegate);
  static WebInterstitial* CreateNativeInterstitial(
      WebState* web_state,
      bool new_navigation,
      const GURL& url,
      std::unique_ptr<NativeWebInterstitialDelegate> delegate);

  // Retrieves the WebInterstitial if any associated with the specified
  // |web_state|.
  static WebInterstitial* GetWebInterstitial(WebState* web_state);

  virtual ~WebInterstitial() {}

  // Shows the interstitial page in the WebState.
  virtual void Show() = 0;

  // Hides the interstitial page.
  virtual void Hide() = 0;

  // Reverts to the page showing before the interstitial.
  // Delegates should call this method when the user has chosen NOT to proceed
  // to the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void DontProceed() = 0;

  // Delegates should call this method when the user has chosen to proceed to
  // the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void Proceed() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INTERSTITIALS_WEB_INTERSTITIAL_H_
