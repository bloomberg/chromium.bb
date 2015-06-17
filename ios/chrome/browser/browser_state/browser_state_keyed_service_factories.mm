// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/browser_state_keyed_service_factories.h"

#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/chrome/browser/enhanced_bookmarks/bookmark_server_cluster_service_factory.h"
#include "ios/chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#include "ios/chrome/browser/translate/translate_accept_languages_factory.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes itself and registers its dependencies with
// the global PreferenceDependencyManager. We need to have a complete
// dependency graph when we create a browser state so we can dispatch the
// browser state creation message to the services that want to create their
// services at browser state creation time.
//
// TODO(erg): This needs to be something else. I don't think putting every
// FooServiceFactory here will scale or is desirable long term.
void EnsureBrowserStateKeyedServiceFactoriesBuilt() {
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  enhanced_bookmarks::BookmarkServerClusterServiceFactory::GetInstance();
  enhanced_bookmarks::EnhancedBookmarkModelFactory::GetInstance();
  suggestions::SuggestionsServiceFactory::GetInstance();
  TranslateAcceptLanguagesFactory::GetInstance();

  if (ios::GetKeyedServiceProvider())
    ios::GetKeyedServiceProvider()->AssertKeyedFactoriesBuilt();
}
