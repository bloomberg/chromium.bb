// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/permissions/permission_context_factory_base.h"
#include "components/prefs/pref_service.h"

class GeolocationPermissionContext;
class PrefRegistrySyncable;
class Profile;

class GeolocationPermissionContextFactory
    : public PermissionContextFactoryBase {
 public:
  static GeolocationPermissionContext* GetForProfile(Profile* profile);

  static GeolocationPermissionContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      GeolocationPermissionContextFactory>;

  GeolocationPermissionContextFactory();
  ~GeolocationPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextFactory);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
