// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

class ChromiumBrowserProvider : public ios::ChromeBrowserProvider {
 public:
  ChromiumBrowserProvider();
  ~ChromiumBrowserProvider() override;

 private:
  // ChromeBrowserProvider implementation
  VoiceSearchProvider* GetVoiceSearchProvider() const override;
  id<LogoVendor> CreateLogoVendor(ios::ChromeBrowserState* browser_state,
                                  id<UrlLoader> loader) const override
      NS_RETURNS_RETAINED;

  std::unique_ptr<VoiceSearchProvider> voice_search_provider_;
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
