// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/blimp/blimp_client_context_factory.h"

#include "base/memory/singleton.h"
#include "base/supports_user_data.h"
#include "blimp/client/public/blimp_client_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

// static
BlimpClientContextFactory* BlimpClientContextFactory::GetInstance() {
  return base::Singleton<BlimpClientContextFactory>::get();
}

// static
blimp::client::BlimpClientContext*
BlimpClientContextFactory::GetForBrowserContext(
    content::BrowserContext* context) {
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
          content::BrowserThread::IO));
}

content::BrowserContext* BlimpClientContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
