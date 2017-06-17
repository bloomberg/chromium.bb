// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printer_detector/usb_printer_detector_factory.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/printer_detector/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos {

namespace {

base::LazyInstance<UsbPrinterDetectorFactory>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
UsbPrinterDetectorFactory* UsbPrinterDetectorFactory::GetInstance() {
  return g_factory.Pointer();
}

UsbPrinterDetector* UsbPrinterDetectorFactory::Get(
    content::BrowserContext* context) {
  return static_cast<UsbPrinterDetector*>(
      GetServiceForBrowserContext(context, false));
}

UsbPrinterDetectorFactory::UsbPrinterDetectorFactory()
    : BrowserContextKeyedServiceFactory(
          "UsbPrinterDetectorFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(PrintersManagerFactory::GetInstance());
}

UsbPrinterDetectorFactory::~UsbPrinterDetectorFactory() {}

content::BrowserContext* UsbPrinterDetectorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* UsbPrinterDetectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return UsbPrinterDetector::Create(Profile::FromBrowserContext(context))
      .release();
}

bool UsbPrinterDetectorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool UsbPrinterDetectorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
