// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_factory.h"

#include "chrome/browser/extensions/api/bookmarks/bookmark_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
BookmarkAPI* BookmarkAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BookmarkAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
BookmarkAPIFactory* BookmarkAPIFactory::GetInstance() {
  return Singleton<BookmarkAPIFactory>::get();
}

BookmarkAPIFactory::BookmarkAPIFactory()
    : ProfileKeyedServiceFactory("BookmarkAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

BookmarkAPIFactory::~BookmarkAPIFactory() {
}

ProfileKeyedService* BookmarkAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new BookmarkAPI(profile);
}

bool BookmarkAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool BookmarkAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
