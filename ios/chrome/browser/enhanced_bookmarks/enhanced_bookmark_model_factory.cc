// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"

#include "base/memory/singleton.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

namespace enhanced_bookmarks {

// static
EnhancedBookmarkModelFactory* EnhancedBookmarkModelFactory::GetInstance() {
  return base::Singleton<EnhancedBookmarkModelFactory>::get();
}

// static
EnhancedBookmarkModel* EnhancedBookmarkModelFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<EnhancedBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

EnhancedBookmarkModelFactory::EnhancedBookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "EnhancedBookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
}

EnhancedBookmarkModelFactory::~EnhancedBookmarkModelFactory() {
}

scoped_ptr<KeyedService> EnhancedBookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return make_scoped_ptr(new EnhancedBookmarkModel(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state),
      "chrome." + version_info::GetVersionNumber()));
}

web::BrowserState* EnhancedBookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace enhanced_bookmarks
