// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
MediaDrmOriginIdManager* MediaDrmOriginIdManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaDrmOriginIdManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaDrmOriginIdManagerFactory* MediaDrmOriginIdManagerFactory::GetInstance() {
  return base::Singleton<MediaDrmOriginIdManagerFactory>::get();
}

MediaDrmOriginIdManagerFactory::MediaDrmOriginIdManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaDrmOriginIdManager",
          BrowserContextDependencyManager::GetInstance()) {}

MediaDrmOriginIdManagerFactory::~MediaDrmOriginIdManagerFactory() = default;

KeyedService* MediaDrmOriginIdManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new MediaDrmOriginIdManager(profile->GetPrefs());
}

content::BrowserContext* MediaDrmOriginIdManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // No service for Incognito mode.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}
