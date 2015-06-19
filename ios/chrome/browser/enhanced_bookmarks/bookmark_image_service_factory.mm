// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/enhanced_bookmarks/bookmark_image_service_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/enhanced_bookmarks/bookmark_image_service_ios.h"
#include "ios/chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/web_thread.h"

// static
BookmarkImageServiceIOS* BookmarkImageServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<BookmarkImageServiceIOS*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BookmarkImageServiceFactory* BookmarkImageServiceFactory::GetInstance() {
  return Singleton<BookmarkImageServiceFactory>::get();
}

BookmarkImageServiceFactory::BookmarkImageServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkImageService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(enhanced_bookmarks::EnhancedBookmarkModelFactory::GetInstance());
}

BookmarkImageServiceFactory::~BookmarkImageServiceFactory() {
}

scoped_ptr<KeyedService> BookmarkImageServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return make_scoped_ptr(new BookmarkImageServiceIOS(
      browser_state->GetStatePath(),
      enhanced_bookmarks::EnhancedBookmarkModelFactory::GetForBrowserState(
          browser_state),
      browser_state->GetRequestContext(), web::WebThread::GetBlockingPool()));
}

web::BrowserState* BookmarkImageServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}
