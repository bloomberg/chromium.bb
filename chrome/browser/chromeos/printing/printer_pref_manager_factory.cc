// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"

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

base::LazyInstance<PrinterPrefManagerFactory> g_printer_pref_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PrinterPrefManager* PrinterPrefManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrinterPrefManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrinterPrefManagerFactory* PrinterPrefManagerFactory::GetInstance() {
  return g_printer_pref_manager.Pointer();
}

content::BrowserContext* PrinterPrefManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

PrinterPrefManagerFactory::PrinterPrefManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrinterPrefManager",
          BrowserContextDependencyManager::GetInstance()) {}

PrinterPrefManagerFactory::~PrinterPrefManagerFactory() {}

PrinterPrefManager* PrinterPrefManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // TODO(skau): --disable-sync and --enable-native-cups are mutually exclusive
  // until crbug.com/688533 is resolved.
  DCHECK(sync_service);

  std::unique_ptr<PrintersSyncBridge> sync_bridge =
      base::MakeUnique<PrintersSyncBridge>(
          sync_service->GetModelTypeStoreFactory(syncer::PRINTERS),
          base::BindRepeating(
              base::IgnoreResult(&base::debug::DumpWithoutCrashing)));

  return new PrinterPrefManager(profile, std::move(sync_bridge));
}

}  // namespace chromeos
