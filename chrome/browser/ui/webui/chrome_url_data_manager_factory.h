// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

// Singleton that owns all |ChromeURLDataManager|s and associates them with
// |Profile|s.
class ChromeURLDataManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the appropriate |ChromeURLDataManager| for |profile|. Each
  // |Profile| has its own manager so that chrome:// URLs can be directed to
  // the correct UI.
  static ChromeURLDataManager* GetForProfile(Profile* profile);

  static ChromeURLDataManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeURLDataManagerFactory>;

  ChromeURLDataManagerFactory();
  virtual ~ChromeURLDataManagerFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
    Profile* profile) const OVERRIDE;
  virtual bool ServiceHasOwnInstanceInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLDataManagerFactory);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_FACTORY_H_
