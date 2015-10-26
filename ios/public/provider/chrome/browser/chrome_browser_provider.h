// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/favicon_base/favicon_callback.h"

class GURL;
class InfoBarViewDelegate;
class PrefRegistrySimple;
class PrefService;
class ProfileOAuth2TokenServiceIOSProvider;

namespace autofill {
class CardUnmaskPromptController;
class CardUnmaskPromptView;
}

namespace metrics {
class MetricsService;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace rappor {
class RapporService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
class VariationsService;
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
class ChromeBrowserState;
class ChromeBrowserStateManager;
class ChromeIdentityService;
class GeolocationUpdaterProvider;
class StringProvider;
class LiveTabContextProvider;
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
  // Asserts all iOS-specific |BrowserContextKeyedServiceFactory| are built.
  virtual void AssertBrowserContextKeyedFactoriesBuilt();
  // Registers all prefs that will be used via the local state PrefService.
  virtual void RegisterLocalState(PrefRegistrySimple* registry);
  // Registers all prefs that will be used via a PrefService attached to a
  // Profile.
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry);
  // Returns an instance of profile OAuth2 token service provider.
  virtual ProfileOAuth2TokenServiceIOSProvider*
  GetProfileOAuth2TokenServiceIOSProvider();
  // Returns an UpdatableResourceProvider instance.
  virtual UpdatableResourceProvider* GetUpdatableResourceProvider();
  // Returns a ChromeBrowserStateManager instance.
  virtual ChromeBrowserStateManager* GetChromeBrowserStateManager();
  // Returns an infobar view conforming to the InfoBarViewProtocol. The returned
  // object is retained.
  virtual InfoBarViewPlaceholder CreateInfoBarView(
      CGRect frame,
      InfoBarViewDelegate* delegate);
  // Returns an instance of a Chrome identity service.
  virtual ChromeIdentityService* GetChromeIdentityService();
  // Returns an instance of a string provider.
  virtual StringProvider* GetStringProvider();
  // Returns an instance of a LiveTabContextProvider.
  virtual LiveTabContextProvider* GetLiveTabContextProvider();
  virtual GeolocationUpdaterProvider* GetGeolocationUpdaterProvider();
  // Returns the distribution brand code.
  virtual std::string GetDistributionBrandCode();
  // Sets the alpha property of an UIView with an animation.
  virtual void SetUIViewAlphaWithAnimation(UIView* view, float alpha);
  // Returns the metrics service.
  virtual metrics::MetricsService* GetMetricsService();
  // Returns the variations service.
  virtual variations::VariationsService* GetVariationsService();
  // Returns an instance of a CardUnmaskPromptView used to unmask Wallet cards.
  // The view is responsible for its own lifetime.
  virtual autofill::CardUnmaskPromptView* CreateCardUnmaskPromptView(
      autofill::CardUnmaskPromptController* controller);
  // Returns risk data used in Wallet requests.
  virtual std::string GetRiskData();
  // Starts and manages the policy system.
  virtual policy::BrowserPolicyConnector* GetBrowserPolicyConnector();
  // Returns the RapporService. May be null.
  virtual rappor::RapporService* GetRapporService();
  // Returns whether there is an Off-The-Record session active.
  virtual bool IsOffTheRecordSessionActive();
  // Get the favicon for |page_url| and run |callback| with result when loaded.
  // Note. |callback| is always run asynchronously.
  virtual void GetFaviconForURL(
      ChromeBrowserState* browser_state,
      const GURL& page_url,
      const std::vector<int>& desired_sizes_in_pixel,
      const favicon_base::FaviconResultsCallback& callback) const;

  // Returns whether safe browsing is enabled. See the comment on
  // metrics_services_manager_client.h for details on |on_update_callback|.
  virtual bool IsSafeBrowsingEnabled(const base::Closure& on_update_callback);

  // Called when the IOSChromeMetricsServiceClientManager instance is
  // destroyed.
  virtual void OnMetricsServicesManagerClientDestroyed();
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
