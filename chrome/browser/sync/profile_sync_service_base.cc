// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_base.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

// static
ProfileSyncServiceBase* ProfileSyncServiceBase::FromBrowserContext(
    content::BrowserContext* context) {
  return ProfileSyncServiceFactory::GetForProfile(
      static_cast<Profile*>(context));
}
