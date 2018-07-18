// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
gcm::GCMProfileService* WebViewGCMProfileServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<gcm::GCMProfileService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewGCMProfileServiceFactory*
WebViewGCMProfileServiceFactory::GetInstance() {
  return base::Singleton<WebViewGCMProfileServiceFactory>::get();
}

// static
std::string WebViewGCMProfileServiceFactory::GetProductCategoryForSubtypes() {
  return "org.chromium.chromewebview";
}

WebViewGCMProfileServiceFactory::WebViewGCMProfileServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GCMProfileService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewSigninManagerFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
}

WebViewGCMProfileServiceFactory::~WebViewGCMProfileServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewGCMProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<gcm::GCMProfileService>(
      browser_state->GetPrefs(), browser_state->GetStatePath(),
      browser_state->GetRequestContext(),
      browser_state->GetSharedURLLoaderFactory(),
      version_info::Channel::UNKNOWN, GetProductCategoryForSubtypes(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      WebViewSigninManagerFactory::GetForBrowserState(browser_state),
      WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
      base::WrapUnique(new gcm::GCMClientFactory),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
      blocking_task_runner);
}
}  // namespace ios_web_view
