// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "components/arc/common/print.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcPrintService : public KeyedService,
                        public ConnectionObserver<mojom::PrintInstance>,
                        public mojom::PrintHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPrintService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPrintService(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ~ArcPrintService() override;

  // ConnectionObserver<mojom::PrintInstance> override:
  void OnConnectionReady() override;

  // mojom::PrintHost override:
  void Print(mojo::ScopedHandle pdf_data) override;

 private:
  // Opens the pdf file at |file_path|.
  // If given |file_path| is nullopt, do nothing.
  void OpenPdf(base::Optional<base::FilePath> file_path) const;

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojo::Binding<mojom::PrintHost> binding_;

  base::WeakPtrFactory<ArcPrintService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArcPrintService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_
