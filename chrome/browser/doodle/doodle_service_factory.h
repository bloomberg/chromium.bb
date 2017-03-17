// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOODLE_DOODLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOODLE_DOODLE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace doodle {
class DoodleService;
}  // namespace doodle

class DoodleServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DoodleServiceFactory* GetInstance();
  static doodle::DoodleService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<DoodleServiceFactory>;

  DoodleServiceFactory();
  ~DoodleServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(DoodleServiceFactory);
};

#endif  // CHROME_BROWSER_DOODLE_DOODLE_SERVICE_FACTORY_H_
