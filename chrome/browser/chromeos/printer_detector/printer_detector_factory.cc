// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printer_detector/printer_detector_factory.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/printer_detector/printer_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos {

namespace {

static base::LazyInstance<PrinterDetectorFactory> g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PrinterDetectorFactory* PrinterDetectorFactory::GetInstance() {
  return g_factory.Pointer();
}

PrinterDetector* PrinterDetectorFactory::Get(content::BrowserContext* context) {
  return static_cast<PrinterDetector*>(
      GetServiceForBrowserContext(context, false));
}

PrinterDetectorFactory::PrinterDetectorFactory()
    : BrowserContextKeyedServiceFactory(
          "PrinterDetectorFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

PrinterDetectorFactory::~PrinterDetectorFactory() {
}

KeyedService* PrinterDetectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableNativeCups)) {
    return PrinterDetector::CreateCups(Profile::FromBrowserContext(context))
        .release();
  } else {
    return PrinterDetector::CreateLegacy(Profile::FromBrowserContext(context))
        .release();
  }
}

bool PrinterDetectorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrinterDetectorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
