// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace gcm {

class PushMessagingPermissionContext;

class PushMessagingPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PushMessagingPermissionContext* GetForProfile(Profile* profile);
  static PushMessagingPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PushMessagingPermissionContextFactory>;

  PushMessagingPermissionContextFactory();
  virtual ~PushMessagingPermissionContextFactory();

  // BrowserContextKeyedBaseFactory methods:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingPermissionContextFactory);
};

}  // namespace gcm
#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_
