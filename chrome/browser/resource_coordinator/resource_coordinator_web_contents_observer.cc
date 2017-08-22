// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_web_contents_observer.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/ukm/ukm_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_callbacks.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ResourceCoordinatorWebContentsObserver);

// We delay the sending of favicon/title update signal to GRC for 5 minutes
// after the main frame navigation is committed.
const base::TimeDelta kFaviconAndTitleUpdateIgnoredTimeout =
    base::TimeDelta::FromMinutes(5);
bool ResourceCoordinatorWebContentsObserver::ukm_recorder_initialized = false;

ResourceCoordinatorWebContentsObserver::ResourceCoordinatorWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();

  tab_resource_coordinator_ =
      base::MakeUnique<resource_coordinator::ResourceCoordinatorInterface>(
          connector, resource_coordinator::CoordinationUnitType::kWebContents);

  // Make sure to set the visibility property when we create
  // |tab_resource_coordinator_|.
  tab_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kVisible,
      web_contents->IsVisible());

  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           mojo::MakeRequest(&service_callbacks_));

  if (base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    EnsureUkmRecorderInterface();
  }
}

// TODO(matthalp) integrate into ResourceCoordinatorService once the UKM mojo
// interface can be accessed directly within GRC. GRC cannot currently use
// the UKM mojo inteface directly because of software layering issues
// (i.e. the UKM mojo interface is only reachable from the content_browser
// service which cannot be accesses by services in the services/ directory).
// Instead the ResourceCoordinatorWebContentsObserver is used as it lives
// within the content_browser service and can initialize the
// UkmRecorderInterface and pass that interface into GRC through
// resource_coordinator::mojom::ServiceCallbacks.
void ResourceCoordinatorWebContentsObserver::EnsureUkmRecorderInterface() {
  if (ukm_recorder_initialized) {
    return;
  }

  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           mojo::MakeRequest(&service_callbacks_));

  service_callbacks_->IsUkmRecorderInterfaceInitialized(base::Bind(
      &ResourceCoordinatorWebContentsObserver::MaybeSetUkmRecorderInterface,
      base::Unretained(this)));
}

void ResourceCoordinatorWebContentsObserver::MaybeSetUkmRecorderInterface(
    bool ukm_recorder_already_initialized) {
  if (ukm_recorder_already_initialized) {
    return;
  }

  ukm::mojom::UkmRecorderInterfacePtr ukm_recorder_interface;
  ukm::UkmInterface::Create(g_browser_process->ukm_recorder(),
                            mojo::MakeRequest(&ukm_recorder_interface));

  service_callbacks_->SetUkmRecorderInterface(
      std::move(ukm_recorder_interface));
  ukm_recorder_initialized = true;
}

ResourceCoordinatorWebContentsObserver::
    ~ResourceCoordinatorWebContentsObserver() = default;

// static
bool ResourceCoordinatorWebContentsObserver::IsEnabled() {
  // Check that service_manager is active and GRC is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled();
}

void ResourceCoordinatorWebContentsObserver::WasShown() {
  tab_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kVisible, true);
}

void ResourceCoordinatorWebContentsObserver::WasHidden() {
  tab_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kVisible, false);
}

void ResourceCoordinatorWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_handle->IsInMainFrame()) {
    UpdateUkmRecorder(navigation_handle->GetNavigationId());
    ResetFlag();
    navigation_finished_time_ = base::TimeTicks::Now();
  }

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();

  auto* frame_resource_coordinator =
      render_frame_host->GetFrameResourceCoordinator();
  tab_resource_coordinator_->AddChild(*frame_resource_coordinator);

  auto* process_resource_coordinator =
      render_frame_host->GetProcess()->GetProcessResourceCoordinator();
  process_resource_coordinator->AddChild(*frame_resource_coordinator);
}

void ResourceCoordinatorWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry,
    bool explicit_set) {
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  // Ignore update when the tab is in foreground or the update happens within 5
  // minutes after the main frame is committed.
  if (web_contents()->IsVisible() ||
      base::TimeTicks::Now() - navigation_finished_time_ <
          kFaviconAndTitleUpdateIgnoredTimeout) {
    return;
  }
  tab_resource_coordinator_->SendEvent(
      resource_coordinator::mojom::Event::kTitleUpdated);
}

void ResourceCoordinatorWebContentsObserver::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  // Ignore update when the tab is in foreground or the update happens within 5
  // minutes after the main frame is committed.
  if (web_contents()->IsVisible() ||
      base::TimeTicks::Now() - navigation_finished_time_ <
          kFaviconAndTitleUpdateIgnoredTimeout) {
    return;
  }
  tab_resource_coordinator_->SendEvent(
      resource_coordinator::mojom::Event::kFaviconUpdated);
}

void ResourceCoordinatorWebContentsObserver::UpdateUkmRecorder(
    int64_t navigation_id) {
  if (!base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    return;
  }

  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  tab_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kUKMSourceId, ukm_source_id_);
}

void ResourceCoordinatorWebContentsObserver::ResetFlag() {
  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}
