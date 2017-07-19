// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_grc_tab_signal_observer.h"

#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace resource_coordinator {

TabManager::GRCTabSignalObserver::GRCTabSignalObserver() : binding_(this) {
  content::ServiceManagerConnection* service_manager_connection =
      content::ServiceManagerConnection::GetForProcess();
  // Ensure service_manager is active before trying to connect to it.
  if (service_manager_connection) {
    service_manager::Connector* connector =
        service_manager_connection->GetConnector();
    mojom::TabSignalGeneratorPtr tab_signal_generator_ptr;
    connector->BindInterface(mojom::kServiceName,
                             mojo::MakeRequest(&tab_signal_generator_ptr));
    mojom::TabSignalObserverPtr tab_signal_observer_ptr;
    binding_.Bind(mojo::MakeRequest(&tab_signal_observer_ptr));
    tab_signal_generator_ptr->AddObserver(std::move(tab_signal_observer_ptr));
  }
}

TabManager::GRCTabSignalObserver::~GRCTabSignalObserver() = default;

// static
bool TabManager::GRCTabSignalObserver::IsEnabled() {
  // Check that service_manager is active and GRC is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled();
}

void TabManager::GRCTabSignalObserver::OnEventReceived(
    const CoordinationUnitID& cu_id,
    mojom::TabEvent event) {
  if (event == mojom::TabEvent::kDoneLoading) {
    auto web_contents_iter = cu_id_web_contents_map_.find(cu_id);
    if (web_contents_iter == cu_id_web_contents_map_.end())
      return;
    auto* web_contents_data =
        TabManager::WebContentsData::FromWebContents(web_contents_iter->second);
    web_contents_data->DoneLoading();
  }
}

void TabManager::GRCTabSignalObserver::
    AssociateCoordinationUnitIDWithWebContents(
        const CoordinationUnitID& cu_id,
        content::WebContents* web_contents) {
  cu_id_web_contents_map_[cu_id] = web_contents;
}

void TabManager::GRCTabSignalObserver::RemoveCoordinationUnitID(
    const CoordinationUnitID& cu_id) {
  cu_id_web_contents_map_.erase(cu_id);
}

}  // namespace resource_coordinator
