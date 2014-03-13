// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ProtectedMediaIdentifierPermissionContext;
class Profile;

class ProtectedMediaIdentifierPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ProtectedMediaIdentifierPermissionContext* GetForProfile(
      Profile* profile);

  static ProtectedMediaIdentifierPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<
      ProtectedMediaIdentifierPermissionContextFactory>;

  ProtectedMediaIdentifierPermissionContextFactory();
  virtual ~ProtectedMediaIdentifierPermissionContextFactory();

  // BrowserContextKeyedBaseFactory methods:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(
      ProtectedMediaIdentifierPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_
