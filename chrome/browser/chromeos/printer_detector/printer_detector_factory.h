// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_system_provider.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class PrinterDetector;

class PrinterDetectorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrinterDetectorFactory* GetInstance();

  PrinterDetector* Get(content::BrowserContext* context);

 private:
  friend struct base::DefaultLazyInstanceTraits<PrinterDetectorFactory>;
  PrinterDetectorFactory();
  ~PrinterDetectorFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(PrinterDetectorFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTER_DETECTOR_PRINTER_DETECTOR_FACTORY_H_
