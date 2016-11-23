// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/favicon_base/favicon_callback.h"

class AppDistributionProvider;
class AutocompleteProvider;
class BrandedImageProvider;
class GURL;
class InfoBarViewDelegate;
class OmahaServiceProvider;
class PrefRegistrySimple;
class PrefService;
class UserFeedbackProvider;
class VoiceSearchProvider;

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

namespace sync_sessions {
class SyncedWindowDelegatesGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

@protocol AppRatingPrompt;
@protocol InfoBarViewProtocol;
@protocol LogoVendor;
@protocol TextFieldStyling;
@protocol NativeAppWhitelistManager;
@class UITextField;
@class UIView;
@protocol UrlLoader;
typedef UIView<InfoBarViewProtocol>* InfoBarViewPlaceholder;

namespace ios {

class ChromeBrowserProvider;
class ChromeBrowserState;
class ChromeIdentityService;
class GeolocationUpdaterProvider;
class SigninErrorProvider;
class SigninResourcesProvider;
class LiveTabContextProvider;

// Setter and getter for the provider. The provider should be set early, before
// any browser code is called.
void SetChromeBrowserProvider(ChromeBrowserProvider* provider);
ChromeBrowserProvider* GetChromeBrowserProvider();

// Factory function for the embedder specific provider. This function must be
// implemented by the embedder and will be selected via linking (i.e. by the
// build system). Should only be used in the application startup code, not by
// the tests (as they may use a different provider).
std::unique_ptr<ChromeBrowserProvider> CreateChromeBrowserProvider();

// A class that allows embedding iOS-specific functionality in the
// ios_chrome_browser target.
class ChromeBrowserProvider {
 public:
  // The constructor is called before web startup.
  ChromeBrowserProvider();
  virtual ~ChromeBrowserProvider();

  // This is called after web startup.
  virtual void Initialize() const;

  // Asserts all iOS-specific |BrowserContextKeyedServiceFactory| are built.
  virtual void AssertBrowserContextKeyedFactoriesBuilt();
  // Registers all prefs that will be used via a PrefService attached to a
  // Profile.
  virtual void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  // Returns an infobar view conforming to the InfoBarViewProtocol. The returned
  // object is retained.
  virtual InfoBarViewPlaceholder CreateInfoBarView(
      CGRect frame,
      InfoBarViewDelegate* delegate) NS_RETURNS_RETAINED;
  // Returns an instance of a signing error provider.
  virtual SigninErrorProvider* GetSigninErrorProvider();
  // Returns an instance of a signin resources provider.
  virtual SigninResourcesProvider* GetSigninResourcesProvider();
  // Sets the current instance of Chrome identity service. Used for testing.
  virtual void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service);
  // Returns an instance of a Chrome identity service.
  virtual ChromeIdentityService* GetChromeIdentityService();
  // Returns an instance of a LiveTabContextProvider.
  virtual LiveTabContextProvider* GetLiveTabContextProvider();
  virtual GeolocationUpdaterProvider* GetGeolocationUpdaterProvider();
  // Returns "enabled", "disabled", or "default".
  virtual std::string DataReductionProxyAvailability();
  // Returns the distribution brand code.
  virtual std::string GetDistributionBrandCode();
  // Sets the alpha property of an UIView with an animation.
  virtual void SetUIViewAlphaWithAnimation(UIView* view, float alpha);
  // Returns an instance of a CardUnmaskPromptView used to unmask Wallet cards.
  // The view is responsible for its own lifetime.
  virtual autofill::CardUnmaskPromptView* CreateCardUnmaskPromptView(
      autofill::CardUnmaskPromptController* controller);
  // Returns risk data used in Wallet requests.
  virtual std::string GetRiskData();
  // Returns whether there is an Off-The-Record session active.
  virtual bool IsOffTheRecordSessionActive();
  // Get the favicon for |page_url| and run |callback| with result when loaded.
  // Note. |callback| is always run asynchronously.
  virtual void GetFaviconForURL(
      ChromeBrowserState* browser_state,
      const GURL& page_url,
      const std::vector<int>& desired_sizes_in_pixel,
      const favicon_base::FaviconResultsCallback& callback) const;
  // Creates and returns a new styled text field with the given |frame|.
  virtual UITextField<TextFieldStyling>* CreateStyledTextField(
      CGRect frame) const NS_RETURNS_RETAINED;
  // Creates and returns an app ratings prompt object.  Can return nil if app
  // ratings prompts are not supported by the provider.
  virtual id<AppRatingPrompt> CreateAppRatingPrompt() const NS_RETURNS_RETAINED;

  // Initializes the cast service.  Should be called soon after the given
  // |main_tab_model| is created.
  // TODO(rohitrao): Change from |id| to |TabModel*| once TabModel is moved into
  // the Chromium tree.
  virtual void InitializeCastService(id main_tab_model) const;

  // Attaches any embedder-specific tab helpers to the given |web_state|.  The
  // owning |tab| is included for helpers that need access to information that
  // is not yet available through web::WebState.
  // TODO(rohitrao): Change from |id| to |Tab*| once Tab is moved into the
  // Chromium tree.
  virtual void AttachTabHelpers(web::WebState* web_state, id tab) const;

  // Returns whether safe browsing is enabled. See the comment on
  // metrics_services_manager_client.h for details on |on_update_callback|.
  virtual bool IsSafeBrowsingEnabled(const base::Closure& on_update_callback);

  // Returns an instance of the voice search provider, if one exists.
  virtual VoiceSearchProvider* GetVoiceSearchProvider() const;

  // Returns an instance of the app distribution provider.
  virtual AppDistributionProvider* GetAppDistributionProvider() const;

  // Creates and returns an object that can fetch and vend search engine logos.
  // The caller assumes ownership of the returned object.
  virtual id<LogoVendor> CreateLogoVendor(
      ios::ChromeBrowserState* browser_state,
      id<UrlLoader> loader) const NS_RETURNS_RETAINED;

  // Returns an instance of the omaha service provider.
  virtual OmahaServiceProvider* GetOmahaServiceProvider() const;

  // Returns an instance of the user feedback provider.
  virtual UserFeedbackProvider* GetUserFeedbackProvider() const;

  // Returns the SyncedWindowDelegatesGetter implementation.
  virtual std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
  CreateSyncedWindowDelegatesGetter(ios::ChromeBrowserState* browser_state);

  // Returns an instance of the branded image provider.
  virtual BrandedImageProvider* GetBrandedImageProvider() const;

  // Returns the NativeAppWhitelistManager implementation.
  virtual id<NativeAppWhitelistManager> GetNativeAppWhitelistManager() const;

  // Hides immediately the modals related to this provider.
  virtual void HideModalViewStack() const;

  // Logs if any modals created by this provider are still presented. It does
  // not dismiss them.
  virtual void LogIfModalViewsArePresented() const;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
