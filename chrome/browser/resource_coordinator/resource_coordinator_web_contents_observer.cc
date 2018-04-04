// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_web_contents_observer.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/resource_coordinator/public/cpp/page_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// The manager currently doesn't exist on all platforms, which means the
// tab load tracker will not either.
// TODO(chrisha): Make the tab manager exist everywhere. It's going to start
// making scheduling decisions that apply to mobile devices as well, so there's
// no longer any reason for it to be mobile only.
resource_coordinator::TabLoadTracker* GetTabLoadTracker() {
  if (auto* manager = g_browser_process->GetTabManager())
    return &(manager->tab_load_tracker());
  return nullptr;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ResourceCoordinatorWebContentsObserver);

ResourceCoordinatorWebContentsObserver::ResourceCoordinatorWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  service_manager::Connector* connector = nullptr;
  // |ServiceManagerConnection| is null in test.
  if (content::ServiceManagerConnection::GetForProcess()) {
    connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
  }

  page_resource_coordinator_ =
      std::make_unique<resource_coordinator::PageResourceCoordinator>(
          connector);

  // Make sure to set the visibility property when we create
  // |page_resource_coordinator_|.
  const bool is_visible =
      web_contents->GetVisibility() != content::Visibility::HIDDEN;
  page_resource_coordinator_->SetVisibility(is_visible);

  if (auto* page_signal_receiver =
          resource_coordinator::PageSignalReceiver::GetInstance()) {
    // Gets CoordinationUnitID for this WebContents and adds it to
    // PageSignalReceiver.
    page_signal_receiver->AssociateCoordinationUnitIDWithWebContents(
        page_resource_coordinator_->id(), web_contents);
  }

  if (auto* tracker = GetTabLoadTracker())
    tracker->StartTracking(web_contents);
}

ResourceCoordinatorWebContentsObserver::
    ~ResourceCoordinatorWebContentsObserver() = default;

// static
bool ResourceCoordinatorWebContentsObserver::IsEnabled() {
  // Check that service_manager is active and GRC is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled();
}

void ResourceCoordinatorWebContentsObserver::DidStartLoading() {
  page_resource_coordinator_->SetIsLoading(true);

  if (auto* tracker = GetTabLoadTracker())
    tracker->DidStartLoading(web_contents());
}

void ResourceCoordinatorWebContentsObserver::DidReceiveResponse() {
  if (auto* tracker = GetTabLoadTracker())
    tracker->DidReceiveResponse(web_contents());
}

void ResourceCoordinatorWebContentsObserver::DidStopLoading() {
  page_resource_coordinator_->SetIsLoading(false);
  if (auto* tracker = GetTabLoadTracker())
    tracker->DidStopLoading(web_contents());
}

void ResourceCoordinatorWebContentsObserver::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  if (auto* tracker = GetTabLoadTracker())
    tracker->DidFailLoad(web_contents());
}

void ResourceCoordinatorWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  // TODO(fdoray): An OCCLUDED tab should not be considered visible.
  const bool is_visible = visibility != content::Visibility::HIDDEN;
  page_resource_coordinator_->SetVisibility(is_visible);
}

void ResourceCoordinatorWebContentsObserver::WebContentsDestroyed() {
  if (auto* page_signal_receiver =
          resource_coordinator::PageSignalReceiver::GetInstance()) {
    // Gets CoordinationUnitID for this WebContents and removes it from
    // PageSignalReceiver.
    page_signal_receiver->RemoveCoordinationUnitID(
        page_resource_coordinator_->id());
  }
  if (auto* tracker = GetTabLoadTracker())
    tracker->StopTracking(web_contents());
}

void ResourceCoordinatorWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  // Make sure the hierarchical structure is constructured before sending signal
  // to Resource Coordinator.
  auto* frame_resource_coordinator =
      render_frame_host->GetFrameResourceCoordinator();
  page_resource_coordinator_->AddFrame(*frame_resource_coordinator);

  auto* process_resource_coordinator =
      render_frame_host->GetProcess()->GetProcessResourceCoordinator();
  process_resource_coordinator->AddFrame(*frame_resource_coordinator);

  if (navigation_handle->IsInMainFrame()) {
    UpdateUkmRecorder(navigation_handle->GetNavigationId());
    ResetFlag();
    page_resource_coordinator_->OnMainFrameNavigationCommitted();
  }
}

void ResourceCoordinatorWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  page_resource_coordinator_->OnTitleUpdated();
}

void ResourceCoordinatorWebContentsObserver::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  page_resource_coordinator_->OnFaviconUpdated();
}

void ResourceCoordinatorWebContentsObserver::UpdateUkmRecorder(
    int64_t navigation_id) {
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  page_resource_coordinator_->SetUKMSourceId(ukm_source_id_);
}

void ResourceCoordinatorWebContentsObserver::ResetFlag() {
  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}
