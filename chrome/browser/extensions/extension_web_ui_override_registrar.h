// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionWebUIOverrideRegistrar : public BrowserContextKeyedAPI,
                                        public content::NotificationObserver {
 public:
  explicit ExtensionWebUIOverrideRegistrar(content::BrowserContext* context);
  virtual ~ExtensionWebUIOverrideRegistrar();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
      GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionWebUIOverrideRegistrar";
  }

  Profile* const profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebUIOverrideRegistrar);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
