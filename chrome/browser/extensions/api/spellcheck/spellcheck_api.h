// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class Profile;

namespace extensions {

class SpellcheckAPI : public BrowserContextKeyedAPI,
                      public content::NotificationObserver {
 public:
  explicit SpellcheckAPI(content::BrowserContext* context);
  virtual ~SpellcheckAPI();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SpellcheckAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SpellcheckAPI>;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SpellcheckAPI";
  }

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckAPI);
};

template <>
void BrowserContextKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
