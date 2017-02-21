// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/web/public/web_thread.h"

// static
favicon::LargeIconService* IOSChromeLargeIconServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<favicon::LargeIconService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeLargeIconServiceFactory*
IOSChromeLargeIconServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeLargeIconServiceFactory>::get();
}

IOSChromeLargeIconServiceFactory::IOSChromeLargeIconServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "LargeIconService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::FaviconServiceFactory::GetInstance());
}

IOSChromeLargeIconServiceFactory::~IOSChromeLargeIconServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeLargeIconServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  return base::MakeUnique<favicon::LargeIconService>(
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      web::WebThread::GetBlockingPool());
}

web::BrowserState* IOSChromeLargeIconServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool IOSChromeLargeIconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
