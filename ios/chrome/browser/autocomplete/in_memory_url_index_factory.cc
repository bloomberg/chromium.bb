// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/web_thread.h"

namespace ios {

namespace {

scoped_ptr<KeyedService> BuildInMemoryURLIndex(web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  SchemeSet schemes_to_whilelist;
  schemes_to_whilelist.insert(kChromeUIScheme);

  // Do not force creation of the HistoryService if saving history is disabled.
  scoped_ptr<InMemoryURLIndex> in_memory_url_index(new InMemoryURLIndex(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state),
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS),
      web::WebThread::GetBlockingPool(), browser_state->GetStatePath(),
      browser_state->GetPrefs()->GetString(prefs::kAcceptLanguages),
      schemes_to_whilelist));
  in_memory_url_index->Init();
  return std::move(in_memory_url_index);
}

}  // namespace

// static
InMemoryURLIndex* InMemoryURLIndexFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<InMemoryURLIndex*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
InMemoryURLIndexFactory* InMemoryURLIndexFactory::GetInstance() {
  return base::Singleton<InMemoryURLIndexFactory>::get();
}

InMemoryURLIndexFactory::InMemoryURLIndexFactory()
    : BrowserStateKeyedServiceFactory(
          "InMemoryURLIndex",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

InMemoryURLIndexFactory::~InMemoryURLIndexFactory() {}

// static
BrowserStateKeyedServiceFactory::TestingFactoryFunction
InMemoryURLIndexFactory::GetDefaultFactory() {
  return &BuildInMemoryURLIndex;
}

scoped_ptr<KeyedService> InMemoryURLIndexFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildInMemoryURLIndex(context);
}

web::BrowserState* InMemoryURLIndexFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool InMemoryURLIndexFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
