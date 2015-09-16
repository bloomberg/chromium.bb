// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/permissions/permission_context_factory_base.h"

class ProtectedMediaIdentifierPermissionContext;
class Profile;

class ProtectedMediaIdentifierPermissionContextFactory
    : public PermissionContextFactoryBase {
 public:
  static ProtectedMediaIdentifierPermissionContext* GetForProfile(
      Profile* profile);

  static ProtectedMediaIdentifierPermissionContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ProtectedMediaIdentifierPermissionContextFactory>;

  ProtectedMediaIdentifierPermissionContextFactory();
  ~ProtectedMediaIdentifierPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(
      ProtectedMediaIdentifierPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_FACTORY_H_
