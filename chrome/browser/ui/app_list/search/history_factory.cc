// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/history_factory.h"

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/search/common/dictionary_data_store.h"
#include "chrome/browser/ui/app_list/search/history.h"
#include "chrome/browser/ui/app_list/search/history_data_store.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

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
  const char kStoreDataFileName[] = "App Launcher Search";
  const base::FilePath data_file =
      context->GetPath().AppendASCII(kStoreDataFileName);
  scoped_refptr<DictionaryDataStore> dictionary_data_store(
      new DictionaryDataStore(data_file));
  scoped_refptr<HistoryDataStore> history_data_store(
      new HistoryDataStore(dictionary_data_store));
  return new History(history_data_store);
}

}  // namespace app_list
