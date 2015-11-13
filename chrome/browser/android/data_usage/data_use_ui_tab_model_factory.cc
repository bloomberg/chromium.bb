// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace android {

// static
DataUseUITabModelFactory* DataUseUITabModelFactory::GetInstance() {
  return base::Singleton<DataUseUITabModelFactory>::get();
}

// static
DataUseUITabModel* DataUseUITabModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataUseUITabModel*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DataUseUITabModelFactory::DataUseUITabModelFactory()
    : BrowserContextKeyedServiceFactory(
          "data_usage::DataUseUITabModel",
          BrowserContextDependencyManager::GetInstance()) {}

DataUseUITabModelFactory::~DataUseUITabModelFactory() {}

KeyedService* DataUseUITabModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new DataUseUITabModel(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
}

}  // namespace android

}  // namespace chrome
