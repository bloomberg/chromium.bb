// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace resource_coordinator {

namespace {
base::LazyInstance<TabManager::PageSignalReceiver>::Leaky
    g_page_signal_receiver;
}

// static
bool TabManager::PageSignalReceiver::IsEnabled() {
  // Check that service_manager is active and GRC is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled();
}

// static
TabManager::PageSignalReceiver* TabManager::PageSignalReceiver::GetInstance() {
  if (!IsEnabled())
    return nullptr;
  return g_page_signal_receiver.Pointer();
}

TabManager::PageSignalReceiver::PageSignalReceiver() : binding_(this) {
  content::ServiceManagerConnection* service_manager_connection =
      content::ServiceManagerConnection::GetForProcess();
  // Ensure service_manager is active before trying to connect to it.
  if (service_manager_connection) {
    service_manager::Connector* connector =
        service_manager_connection->GetConnector();
    mojom::PageSignalGeneratorPtr page_signal_generator_ptr;
    connector->BindInterface(mojom::kServiceName,
                             mojo::MakeRequest(&page_signal_generator_ptr));
    mojom::PageSignalReceiverPtr page_signal_receiver_ptr;
    binding_.Bind(mojo::MakeRequest(&page_signal_receiver_ptr));
    page_signal_generator_ptr->AddReceiver(std::move(page_signal_receiver_ptr));
  }
}

TabManager::PageSignalReceiver::~PageSignalReceiver() = default;

void TabManager::PageSignalReceiver::NotifyPageAlmostIdle(
    const CoordinationUnitID& cu_id) {
  auto web_contents_iter = cu_id_web_contents_map_.find(cu_id);
  if (web_contents_iter == cu_id_web_contents_map_.end())
    return;
  auto* web_contents_data =
      TabManager::WebContentsData::FromWebContents(web_contents_iter->second);
  web_contents_data->NotifyAlmostIdle();
}

void TabManager::PageSignalReceiver::SetExpectedTaskQueueingDuration(
    const CoordinationUnitID& cu_id,
    base::TimeDelta duration) {
  auto web_contents_iter = cu_id_web_contents_map_.find(cu_id);
  if (web_contents_iter == cu_id_web_contents_map_.end())
    return;
  g_browser_process->GetTabManager()
      ->stats_collector()
      ->RecordExpectedTaskQueueingDuration(web_contents_iter->second, duration);
}

void TabManager::PageSignalReceiver::AssociateCoordinationUnitIDWithWebContents(
    const CoordinationUnitID& cu_id,
    content::WebContents* web_contents) {
  cu_id_web_contents_map_[cu_id] = web_contents;
}

void TabManager::PageSignalReceiver::RemoveCoordinationUnitID(
    const CoordinationUnitID& cu_id) {
  cu_id_web_contents_map_.erase(cu_id);
}

}  // namespace resource_coordinator
