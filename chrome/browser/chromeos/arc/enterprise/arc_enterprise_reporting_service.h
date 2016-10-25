// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/enterprise_reporting.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// This class controls the ARC enterprise reporting.
class ArcEnterpriseReportingService
    : public ArcService,
      public InstanceHolder<mojom::EnterpriseReportingInstance>::Observer,
      public mojom::EnterpriseReportingHost {
 public:
  explicit ArcEnterpriseReportingService(ArcBridgeService* arc_bridge_service);
  ~ArcEnterpriseReportingService() override;

  // InstanceHolder<mojom::EnterpriseReportingInstance>::Observer overrides:
  void OnInstanceReady() override;

  // mojom::EnterpriseReportingHost overrides:
  void ReportManagementState(mojom::ManagementState state) override;

 private:
  base::ThreadChecker thread_checker_;

  mojo::Binding<mojom::EnterpriseReportingHost> binding_;

  base::WeakPtrFactory<ArcEnterpriseReportingService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcEnterpriseReportingService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_ENTERPRISE_REPORTING_SERVICE_H_
