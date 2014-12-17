// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

class PrefService;

namespace infobars {
class InfoBarManager;
}

namespace net {
class URLRequestContextGetter;
}

namespace web {
class WebState;
}

// TODO(ios): Determine the best way to interface with Obj-C code through
// the ChromeBrowserProvider. crbug/298181
#ifdef __OBJC__
@class UIView;
@protocol InfoBarViewProtocol;
typedef UIView<InfoBarViewProtocol> InfoBarViewPlaceholder;
#else
class InfoBarViewPlaceholder;
class UIView;
#endif

namespace ios {

class ChromeBrowserProvider;
class StringProvider;

// Setter and getter for the provider. The provider should be set early, before
// any browser code is called.
void SetChromeBrowserProvider(ChromeBrowserProvider* provider);
ChromeBrowserProvider* GetChromeBrowserProvider();

// A class that allows embedding iOS-specific functionality in the
// ios_chrome_browser target.
class ChromeBrowserProvider {
 public:
  ChromeBrowserProvider();
  virtual ~ChromeBrowserProvider();

  // Gets the system URL request context.
  virtual net::URLRequestContextGetter* GetSystemURLRequestContext();
  // Gets the local state.
  virtual PrefService* GetLocalState();
  // Returns an instance of an infobar view. The caller is responsible for
  // initializing the returned object and releasing it when appropriate.
  virtual InfoBarViewPlaceholder* CreateInfoBarView();
  // Gets the infobar manager associated with |web_state|.
  virtual infobars::InfoBarManager* GetInfoBarManager(web::WebState* web_state);
  // Returns an instance of a string provider.
  virtual StringProvider* GetStringProvider();
  // Displays the Translate settings screen.
  virtual void ShowTranslateSettings();
  // Returns the chrome UI scheme.
  // TODO(droger): Remove this method once chrome no longer needs to match
  // content.
  virtual const char* GetChromeUIScheme();
  // Sets the alpha property of an UIView with an animation.
  virtual void SetUIViewAlphaWithAnimation(UIView* view, float alpha);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
