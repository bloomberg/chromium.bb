// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/page_resource_coordinator.h"
#include "base/bind.h"

namespace performance_manager {

PageResourceCoordinator::PageResourceCoordinator(
    PerformanceManager* performance_manager)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  resource_coordinator::CoordinationUnitID new_cu_id(
      resource_coordinator::CoordinationUnitType::kPage,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(performance_manager,
                                                 new_cu_id);
}

PageResourceCoordinator::~PageResourceCoordinator() = default;

void PageResourceCoordinator::SetIsLoading(bool is_loading) {
  if (!service_)
    return;
  service_->SetIsLoading(is_loading);
}

void PageResourceCoordinator::SetVisibility(bool visible) {
  if (!service_)
    return;
  service_->SetVisibility(visible);
}

void PageResourceCoordinator::SetUKMSourceId(int64_t ukm_source_id) {
  if (!service_)
    return;
  service_->SetUKMSourceId(ukm_source_id);
}

void PageResourceCoordinator::OnFaviconUpdated() {
  if (!service_)
    return;
  service_->OnFaviconUpdated();
}

void PageResourceCoordinator::OnTitleUpdated() {
  if (!service_)
    return;
  service_->OnTitleUpdated();
}

void PageResourceCoordinator::OnMainFrameNavigationCommitted(
    base::TimeTicks navigation_committed_time,
    uint64_t navigation_id,
    const std::string& url) {
  if (!service_)
    return;
  service_->OnMainFrameNavigationCommitted(navigation_committed_time,
                                           navigation_id, url);
}

void PageResourceCoordinator::AddFrame(const FrameResourceCoordinator& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!service_ || !frame.service())
    return;
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  frame.service()->GetID(base::BindOnce(&PageResourceCoordinator::AddFrameByID,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void PageResourceCoordinator::RemoveFrame(
    const FrameResourceCoordinator& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!service_ || !frame.service())
    return;
  frame.service()->GetID(
      base::BindOnce(&PageResourceCoordinator::RemoveFrameByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PageResourceCoordinator::ConnectToService(
    resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
    const resource_coordinator::CoordinationUnitID& cu_id) {
  provider->CreatePageCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

void PageResourceCoordinator::AddFrameByID(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  service_->AddFrame(cu_id);
}

void PageResourceCoordinator::RemoveFrameByID(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  service_->RemoveFrame(cu_id);
}

}  // namespace performance_manager
