// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web_data_service_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/web/public/web_thread.h"

namespace ios {
namespace {

void DoNothingOnErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                              sql::InitStatus status,
                              const std::string& diagnostics) {}

}  // namespace

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !browser_state->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !browser_state->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetAutofillWebData() : nullptr;
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetKeywordWebData() : nullptr;
}

// static
scoped_refptr<TokenWebData>
WebDataServiceFactory::GetTokenWebDataForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetTokenWebData() : nullptr;
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return base::Singleton<WebDataServiceFactory>::get();
}

WebDataServiceFactory::WebDataServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "WebDataService",
          BrowserStateDependencyManager::GetInstance()) {
}

WebDataServiceFactory::~WebDataServiceFactory() {
}

std::unique_ptr<KeyedService> WebDataServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  const base::FilePath& browser_state_path = context->GetStatePath();
  return std::make_unique<WebDataServiceWrapper>(
      browser_state_path, GetApplicationContext()->GetApplicationLocale(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
      ios::sync_start_util::GetFlareForSyncableService(browser_state_path),
      &DoNothingOnErrorCallback);
}

web::BrowserState* WebDataServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
