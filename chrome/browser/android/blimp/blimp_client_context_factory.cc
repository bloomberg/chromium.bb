// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/blimp_client_context_factory.h"

#include "base/memory/singleton.h"
#include "base/supports_user_data.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "chrome/browser/android/blimp/chrome_compositor_dependencies.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/android/context_provider_factory.h"

// static
BlimpClientContextFactory* BlimpClientContextFactory::GetInstance() {
  return base::Singleton<BlimpClientContextFactory>::get();
}

// static
blimp::client::BlimpClientContext*
BlimpClientContextFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(!context->IsOffTheRecord());
  return static_cast<blimp::client::BlimpClientContext*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

BlimpClientContextFactory::BlimpClientContextFactory()
    : BrowserContextKeyedServiceFactory(
          "blimp::client::BlimpClientContext",
          BrowserContextDependencyManager::GetInstance()) {}

BlimpClientContextFactory::~BlimpClientContextFactory() {}

KeyedService* BlimpClientContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return blimp::client::BlimpClientContext::Create(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO),
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::FILE),
      base::MakeUnique<ChromeCompositorDependencies>(
          ui::ContextProviderFactory::GetInstance()),
      Profile::FromBrowserContext(context)->GetPrefs());
}

content::BrowserContext* BlimpClientContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
