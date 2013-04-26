// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class SpellcheckAPI : public ProfileKeyedAPI,
                      public content::NotificationObserver {
 public:
  explicit SpellcheckAPI(Profile* profile);
  virtual ~SpellcheckAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SpellcheckAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<SpellcheckAPI>;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SpellcheckAPI";
  }

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckAPI);
};

template <>
void ProfileKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
