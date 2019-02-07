// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_node_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace performance_manager {

GraphNodeProviderImpl::GraphNodeProviderImpl(
    service_manager::ServiceKeepalive* service_keepalive,
    Graph* coordination_unit_graph)
    : service_keepalive_(service_keepalive),
      coordination_unit_graph_(coordination_unit_graph) {
  DCHECK(service_keepalive_);
  keepalive_ref_ = service_keepalive_->CreateRef();
}

GraphNodeProviderImpl::~GraphNodeProviderImpl() = default;

void GraphNodeProviderImpl::OnConnectionError(NodeBase* coordination_unit) {
  coordination_unit->Destruct();
}

void GraphNodeProviderImpl::CreateFrameCoordinationUnit(
    resource_coordinator::mojom::FrameCoordinationUnitRequest request,
    const resource_coordinator::CoordinationUnitID& id) {
  FrameNodeImpl* frame_cu = coordination_unit_graph_->CreateFrameNode(
      id, service_keepalive_->CreateRef());

  frame_cu->Bind(std::move(request));
  auto& frame_cu_binding = frame_cu->binding();

  frame_cu_binding.set_connection_error_handler(
      base::BindOnce(&GraphNodeProviderImpl::OnConnectionError,
                     base::Unretained(this), frame_cu));
}

void GraphNodeProviderImpl::CreatePageCoordinationUnit(
    resource_coordinator::mojom::PageCoordinationUnitRequest request,
    const resource_coordinator::CoordinationUnitID& id) {
  PageNodeImpl* page_cu = coordination_unit_graph_->CreatePageNode(
      id, service_keepalive_->CreateRef());

  page_cu->Bind(std::move(request));
  auto& page_cu_binding = page_cu->binding();

  page_cu_binding.set_connection_error_handler(
      base::BindOnce(&GraphNodeProviderImpl::OnConnectionError,
                     base::Unretained(this), page_cu));
}

void GraphNodeProviderImpl::CreateProcessCoordinationUnit(
    resource_coordinator::mojom::ProcessCoordinationUnitRequest request,
    const resource_coordinator::CoordinationUnitID& id) {
  ProcessNodeImpl* process_cu = coordination_unit_graph_->CreateProcessNode(
      id, service_keepalive_->CreateRef());

  process_cu->Bind(std::move(request));
  auto& process_cu_binding = process_cu->binding();

  process_cu_binding.set_connection_error_handler(
      base::BindOnce(&GraphNodeProviderImpl::OnConnectionError,
                     base::Unretained(this), process_cu));
}

void GraphNodeProviderImpl::GetSystemCoordinationUnit(
    resource_coordinator::mojom::SystemCoordinationUnitRequest request) {
  // Simply fetch the existing SystemCU and add an additional binding to it.
  coordination_unit_graph_
      ->FindOrCreateSystemNode(service_keepalive_->CreateRef())
      ->AddBinding(std::move(request));
}

void GraphNodeProviderImpl::Bind(
    resource_coordinator::mojom::CoordinationUnitProviderRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace performance_manager
