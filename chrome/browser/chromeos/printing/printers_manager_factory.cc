// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_manager_factory.h"

#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

base::LazyInstance<PrintersManagerFactory> g_printers_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PrintersManager* PrintersManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintersManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintersManagerFactory* PrintersManagerFactory::GetInstance() {
  return g_printers_manager.Pointer();
}

content::BrowserContext* PrintersManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

PrintersManagerFactory::PrintersManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintersManager",
          BrowserContextDependencyManager::GetInstance()) {}

PrintersManagerFactory::~PrintersManagerFactory() {}

PrintersManager* PrintersManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // TODO(skym): After crbug.com/688533 is fixed, this should not use
  // CreateInMemoryStoreForTest, but rather a ModelTypeStore creation mechanism
  // that's agnostic to the existence of sync infrastructure.
  const syncer::ModelTypeStoreFactory& store_factory =
      sync_service ? sync_service->GetModelTypeStoreFactory(syncer::PRINTERS)
                   : base::BindRepeating(
                         syncer::ModelTypeStore::CreateInMemoryStoreForTest,
                         syncer::PRINTERS);

  std::unique_ptr<PrintersSyncBridge> sync_bridge =
      base::MakeUnique<PrintersSyncBridge>(
          store_factory,
          base::BindRepeating(
              base::IgnoreResult(&base::debug::DumpWithoutCrashing)));

  return new PrintersManager(profile, std::move(sync_bridge));
}

}  // namespace chromeos
