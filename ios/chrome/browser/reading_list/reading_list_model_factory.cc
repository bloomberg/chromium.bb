// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_model_memory.h"

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ReadingListModelMemory*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ReadingListModelMemory*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
ReadingListModelFactory* ReadingListModelFactory::GetInstance() {
  return base::Singleton<ReadingListModelFactory>::get();
}

ReadingListModelFactory::ReadingListModelFactory()
    : BrowserStateKeyedServiceFactory(
          "ReadingListModel",
          BrowserStateDependencyManager::GetInstance()) {}

ReadingListModelFactory::~ReadingListModelFactory() {}

scoped_ptr<KeyedService> ReadingListModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  scoped_ptr<ReadingListModelMemory> reading_list_model(
      new ReadingListModelMemory());
  return std::move(reading_list_model);
}

web::BrowserState* ReadingListModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
