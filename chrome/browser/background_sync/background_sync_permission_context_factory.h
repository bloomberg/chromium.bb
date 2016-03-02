// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/permissions/permission_context_factory_base.h"

class BackgroundSyncPermissionContext;
class Profile;

class BackgroundSyncPermissionContextFactory
    : public PermissionContextFactoryBase {
 public:
  static BackgroundSyncPermissionContext* GetForProfile(Profile* profile);
  static BackgroundSyncPermissionContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      BackgroundSyncPermissionContextFactory>;

  BackgroundSyncPermissionContextFactory();
  ~BackgroundSyncPermissionContextFactory() override = default;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncPermissionContextFactory);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PERMISSION_CONTEXT_FACTORY_H_
