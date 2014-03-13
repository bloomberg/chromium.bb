// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/history_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/search/history.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace app_list {

// static
HistoryFactory* HistoryFactory::GetInstance() {
  return Singleton<HistoryFactory>::get();
}

// static
History* HistoryFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<History*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

HistoryFactory::HistoryFactory()
    : BrowserContextKeyedServiceFactory(
          "app_list::History",
          BrowserContextDependencyManager::GetInstance()) {}

HistoryFactory::~HistoryFactory() {}

KeyedService* HistoryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new History(context);
}

}  // namespace app_list
