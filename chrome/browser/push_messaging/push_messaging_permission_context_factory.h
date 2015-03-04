// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

class PushMessagingPermissionContext;

class PushMessagingPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PushMessagingPermissionContext* GetForProfile(Profile* profile);
  static PushMessagingPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PushMessagingPermissionContextFactory>;

  PushMessagingPermissionContextFactory();
  ~PushMessagingPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingPermissionContextFactory);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_PERMISSION_CONTEXT_FACTORY_H_
