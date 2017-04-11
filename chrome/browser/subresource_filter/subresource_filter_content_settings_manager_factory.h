// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

// This class is responsible for instantiating a profile-scoped object which
// observes changes to content settings.
class SubresourceFilterContentSettingsManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static void EnsureForProfile(Profile* profile);

  static SubresourceFilterContentSettingsManagerFactory* GetInstance();

  SubresourceFilterContentSettingsManagerFactory();

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManagerFactory);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_FACTORY_H_
