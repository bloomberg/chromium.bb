// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/print.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

class ArcPrintService : public ArcService,
                        public InstanceHolder<mojom::PrintInstance>::Observer,
                        public mojom::PrintHost {
 public:
  explicit ArcPrintService(ArcBridgeService* bridge_service);
  ~ArcPrintService() override;

  // InstanceHolder<mojom::PrintInstance>::Observer:
  void OnInstanceReady() override;

  void Print(mojo::ScopedHandle pdf_data) override;

 private:
  mojo::Binding<mojom::PrintHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcPrintService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_ARC_PRINT_SERVICE_H_
