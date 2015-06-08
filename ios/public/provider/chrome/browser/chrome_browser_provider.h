// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>

#include <string>

class InfoBarViewDelegate;
class PrefService;

namespace autofill {
class CardUnmaskPromptController;
class CardUnmaskPromptView;
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
typedef UIView<InfoBarViewProtocol>* InfoBarViewPlaceholder;
#else
class InfoBarViewPlaceholderClass;
typedef InfoBarViewPlaceholderClass* InfoBarViewPlaceholder;
class UIView;
#endif

namespace ios {

class ChromeBrowserProvider;
class GeolocationUpdaterProvider;
class KeyedServiceProvider;
class StringProvider;
class UpdatableResourceProvider;

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
  // Returns an UpdatableResourceProvider instance.
  virtual UpdatableResourceProvider* GetUpdatableResourceProvider();
  // Returns an infobar view conforming to the InfoBarViewProtocol. The returned
  // object is retained.
  virtual InfoBarViewPlaceholder CreateInfoBarView(
      CGRect frame,
      InfoBarViewDelegate* delegate);
  // Returns an instance of a string provider.
  virtual StringProvider* GetStringProvider();
  virtual GeolocationUpdaterProvider* GetGeolocationUpdaterProvider();
  // Displays the Translate settings screen.
  virtual void ShowTranslateSettings();
  // Returns whether the new bookmark collection experience is enabled.
  virtual bool IsBookmarkCollectionEnabled();
  // Returns the chrome UI scheme.
  // TODO(droger): Remove this method once chrome no longer needs to match
  // content.
  virtual const char* GetChromeUIScheme();
  // Sets the alpha property of an UIView with an animation.
  virtual void SetUIViewAlphaWithAnimation(UIView* view, float alpha);
  // Returns an instance of a CardUnmaskPromptView used to unmask Wallet cards.
  // The view is responsible for its own lifetime.
  virtual autofill::CardUnmaskPromptView*
      CreateCardUnmaskPromptView(
          autofill::CardUnmaskPromptController* controller);
  // Returns risk data used in Wallet requests.
  virtual std::string GetRiskData();
  // Returns product version with prefix.
  virtual std::string GetProductVersionWithPrefix(const std::string& prefix);
  // Returns an instance of KeyedServiceProvider.
  virtual KeyedServiceProvider* GetKeyedServiceProvider();
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
