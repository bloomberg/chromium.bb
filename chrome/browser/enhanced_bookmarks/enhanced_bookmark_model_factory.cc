// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {
const char kVersionPrefix[] = "chrome.";
}

namespace enhanced_bookmarks {

EnhancedBookmarkModelFactory::EnhancedBookmarkModelFactory()
    : BrowserContextKeyedServiceFactory(
          "EnhancedBookmarkModel",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

// static
EnhancedBookmarkModelFactory* EnhancedBookmarkModelFactory::GetInstance() {
  return Singleton<EnhancedBookmarkModelFactory>::get();
}

// static
EnhancedBookmarkModel* EnhancedBookmarkModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(!context->IsOffTheRecord());
  return static_cast<EnhancedBookmarkModel*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* EnhancedBookmarkModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(!browser_context->IsOffTheRecord());
  Profile* profile = Profile::FromBrowserContext(browser_context);

  return new EnhancedBookmarkModel(
      BookmarkModelFactory::GetForProfile(profile),
      kVersionPrefix + chrome::VersionInfo().Version());
}

content::BrowserContext* EnhancedBookmarkModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace enhanced_bookmarks
