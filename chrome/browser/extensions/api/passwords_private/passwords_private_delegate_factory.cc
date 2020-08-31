// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

using content::BrowserContext;

// static
PasswordsPrivateDelegate* PasswordsPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context,
    bool create) {
  return static_cast<PasswordsPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, create));
}

// static
PasswordsPrivateDelegateFactory*
    PasswordsPrivateDelegateFactory::GetInstance() {
  static base::NoDestructor<PasswordsPrivateDelegateFactory> instance;
  return instance.get();
}

PasswordsPrivateDelegateFactory::PasswordsPrivateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordsPrivateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BulkLeakCheckServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PasswordsPrivateDelegateFactory::~PasswordsPrivateDelegateFactory() = default;

KeyedService* PasswordsPrivateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PasswordsPrivateDelegateImpl(static_cast<Profile*>(profile));
}

}  // namespace extensions
