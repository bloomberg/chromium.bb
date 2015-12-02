// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace android {

namespace {

// Returns the weak pointer to DataUseTabModel, and adds |data_use_ui_tab_model|
// as an observer to DataUseTabModel. Must be called only on IO thread.
base::WeakPtr<DataUseTabModel> SetDataUseTabModelOnIOThread(
    IOThread* io_thread,
    base::WeakPtr<DataUseUITabModel> data_use_ui_tab_model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!io_thread || !io_thread->globals())
    return base::WeakPtr<DataUseTabModel>();

  base::WeakPtr<DataUseTabModel> data_use_tab_model =
      io_thread->globals()
          ->external_data_use_observer->GetDataUseTabModel()
          ->GetWeakPtr();
  if (data_use_tab_model)
    data_use_tab_model->AddObserver(data_use_ui_tab_model);
  return data_use_tab_model;
}

}  // namespace

// static
DataUseUITabModelFactory* DataUseUITabModelFactory::GetInstance() {
  return base::Singleton<DataUseUITabModelFactory>::get();
}

// static
DataUseUITabModel* DataUseUITabModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  // Do not create DataUseUITabModel for incognito profiles.
  if (profile->GetOriginalProfile() != profile)
    return nullptr;

  return static_cast<DataUseUITabModel*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DataUseUITabModelFactory::DataUseUITabModelFactory()
    : BrowserContextKeyedServiceFactory(
          "data_usage::DataUseUITabModel",
          BrowserContextDependencyManager::GetInstance()) {}

DataUseUITabModelFactory::~DataUseUITabModelFactory() {}

KeyedService* DataUseUITabModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DataUseUITabModel* data_use_ui_tab_model = new DataUseUITabModel(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));

  // DataUseUITabModel should not be created for incognito profile.
  DCHECK_EQ(Profile::FromBrowserContext(context)->GetOriginalProfile(),
            Profile::FromBrowserContext(context));

  // Pass the DataUseTabModel weak pointer to DataUseUITabModel, and register
  // DataUseUITabModel as a DataUseTabModel::TabDataUseObserver.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SetDataUseTabModelOnIOThread, g_browser_process->io_thread(),
                 data_use_ui_tab_model->GetWeakPtr()),
      base::Bind(&chrome::android::DataUseUITabModel::SetDataUseTabModel,
                 data_use_ui_tab_model->GetWeakPtr()));

  return data_use_ui_tab_model;
}

}  // namespace android

}  // namespace chrome
