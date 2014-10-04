// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {

// Extra support for extensions APIs in Chrome.
class ChromeExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  ChromeExtensionsAPIClient();
  virtual ~ChromeExtensionsAPIClient();

  // ExtensionsApiClient implementation.
  virtual void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<SettingsStorageFactory>& factory,
      const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >& observers,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches)
      override;
  virtual AppViewGuestDelegate* CreateAppViewGuestDelegate() const override;
  virtual ExtensionOptionsGuestDelegate* CreateExtensionOptionsGuestDelegate(
      ExtensionOptionsGuest* guest) const override;
  virtual scoped_ptr<MimeHandlerViewGuestDelegate>
      CreateMimeHandlerViewGuestDelegate(
          MimeHandlerViewGuest* guest) const override;
  virtual WebViewGuestDelegate* CreateWebViewGuestDelegate(
      WebViewGuest* web_view_guest) const override;
  virtual WebViewPermissionHelperDelegate*
      CreateWebViewPermissionHelperDelegate(
          WebViewPermissionHelper* web_view_permission_helper) const override;
  virtual WebRequestEventRouterDelegate* CreateWebRequestEventRouterDelegate()
      const override;
  virtual scoped_refptr<ContentRulesRegistry> CreateContentRulesRegistry(
      content::BrowserContext* browser_context,
      RulesCacheDelegate* cache_delegate) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsAPIClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
