// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/browser/password_store_service.h"
#include "components/sync_driver/sync_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"
#include "ios/web/public/web_thread.h"

// static
scoped_refptr<password_manager::PasswordStore>
IOSChromePasswordStoreFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  password_manager::PasswordStoreService* service =
      static_cast<password_manager::PasswordStoreService*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true));

  return password_manager::GetPasswordStoreFromService(
      service, access_type, browser_state->IsOffTheRecord());
}

// static
IOSChromePasswordStoreFactory* IOSChromePasswordStoreFactory::GetInstance() {
  return base::Singleton<IOSChromePasswordStoreFactory>::get();
}

// static
void IOSChromePasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(
    ios::ChromeBrowserState* browser_state) {
  scoped_refptr<password_manager::PasswordStore> password_store =
      GetForBrowserState(browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  sync_driver::SyncService* sync_service =
      ios::GetKeyedServiceProvider()->GetSyncServiceForBrowserStateIfExists(
          browser_state);
  net::URLRequestContextGetter* request_context_getter =
      browser_state->GetRequestContext();
  password_manager::ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
      password_store.get(), sync_service, request_context_getter,
      browser_state->GetStatePath(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB));
}

IOSChromePasswordStoreFactory::IOSChromePasswordStoreFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

IOSChromePasswordStoreFactory::~IOSChromePasswordStoreFactory() {}

scoped_ptr<KeyedService> IOSChromePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  scoped_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(context->GetStatePath()));

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner(
      base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB));

  scoped_refptr<password_manager::PasswordStore> store =
      new password_manager::PasswordStoreDefault(
          main_thread_runner, db_thread_runner, login_db.Pass());
  return password_manager::BuildServiceInstanceFromStore(
      store, ios::sync_start_util::GetFlareForSyncableService(
                 context->GetStatePath()));
}

web::BrowserState* IOSChromePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
