// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
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
  DCHECK(!context->IsOffTheRecord());
  return static_cast<PrinterPrefManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrinterPrefManagerFactory* PrinterPrefManagerFactory::GetInstance() {
  return g_printer_pref_manager.Pointer();
}

PrinterPrefManagerFactory::PrinterPrefManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrinterPrefManager",
          BrowserContextDependencyManager::GetInstance()) {}

PrinterPrefManagerFactory::~PrinterPrefManagerFactory() {}

PrinterPrefManager* PrinterPrefManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new PrinterPrefManager(profile);
}

}  // namespace chromeos
