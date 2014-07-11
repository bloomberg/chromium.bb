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
      OVERRIDE;
  virtual bool AppViewInternalAttachFrame(
      content::BrowserContext* browser_context,
      const GURL& url,
      int guest_instance_id,
      const std::string& guest_extension_id) OVERRIDE;
  virtual bool AppViewInternalDenyRequest(
      content::BrowserContext* browser_context,
      int guest_instance_id,
      const std::string& guest_extension_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsAPIClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
