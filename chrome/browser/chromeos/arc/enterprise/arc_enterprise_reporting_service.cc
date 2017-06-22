// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/arc_enterprise_reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace arc {

ArcEnterpriseReportingService::ArcEnterpriseReportingService(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_ptr_factory_(this) {
  arc_bridge_service()->enterprise_reporting()->AddObserver(this);
}

ArcEnterpriseReportingService::~ArcEnterpriseReportingService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service()->enterprise_reporting()->RemoveObserver(this);
}

void ArcEnterpriseReportingService::OnInstanceReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->enterprise_reporting(), Init);
  DCHECK(instance);
  mojom::EnterpriseReportingHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance->Init(std::move(host_proxy));
}

void ArcEnterpriseReportingService::ReportManagementState(
    mojom::ManagementState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "ReportManagementState state=" << state;

  if (state == mojom::ManagementState::MANAGED_DO_LOST) {
    DCHECK(ArcServiceManager::Get());
    VLOG(1) << "Management state lost. Removing ARC user data.";
    ArcSessionManager::Get()->RequestArcDataRemoval();
    ArcSessionManager::Get()->StopAndEnableArc();
  }
}

}  // namespace arc
