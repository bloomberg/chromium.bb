// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/new_tab_page_interceptor_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/new_tab_page_interceptor_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

// static
NewTabPageInterceptorServiceFactory*
NewTabPageInterceptorServiceFactory::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return Singleton<NewTabPageInterceptorServiceFactory>::get();
}

// static
NewTabPageInterceptorService*
NewTabPageInterceptorServiceFactory::GetForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<NewTabPageInterceptorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NewTabPageInterceptorServiceFactory::NewTabPageInterceptorServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NTP Request Interceptor Service Factory",
          BrowserContextDependencyManager::GetInstance()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

NewTabPageInterceptorServiceFactory::~NewTabPageInterceptorServiceFactory() {
}

KeyedService* NewTabPageInterceptorServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(context);
  return new NewTabPageInterceptorService(profile);
}
