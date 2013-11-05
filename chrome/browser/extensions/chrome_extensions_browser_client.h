// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "extensions/browser/extensions_browser_client.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Implementation of extensions::BrowserClient for Chrome, which includes
// knowledge of Profiles, BrowserContexts and incognito.
//
// NOTE: Methods that do not require knowledge of browser concepts should be
// implemented in ChromeExtensionsClient even if they are only used in the
// browser process (see chrome/common/extensions/chrome_extensions_client.h).
class ChromeExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  ChromeExtensionsBrowserClient();
  virtual ~ChromeExtensionsBrowserClient();

  // BrowserClient overrides:
  virtual bool IsShuttingDown() OVERRIDE;
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) OVERRIDE;
  virtual bool HasOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) OVERRIDE;
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) OVERRIDE;
  virtual bool DeferLoadingBackgroundHosts(
      content::BrowserContext* context) const OVERRIDE;

  // Get the LazyInstance for ChromeBrowserClient.
  static ChromeExtensionsBrowserClient* GetInstance();

 private:
  friend struct base::DefaultLazyInstanceTraits<ChromeExtensionsBrowserClient>;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSIONS_BROWSER_CLIENT_H_
