// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/node_base.h"

#include <utility>

#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"

namespace performance_manager {

NodeBase::NodeBase(const resource_coordinator::CoordinationUnitID& id,
                   Graph* graph)
    : graph_(graph), id_(id.type, id.id) {
}

NodeBase::~NodeBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NodeBase::BeforeDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnBeforeNodeDestroyed(this);
}

void NodeBase::AddObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void NodeBase::RemoveObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool NodeBase::GetProperty(
    const resource_coordinator::mojom::PropertyType property_type,
    int64_t* result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto value_it = properties_.find(property_type);
  if (value_it != properties_.end()) {
    *result = value_it->second;
    return true;
  }
  return false;
}

int64_t NodeBase::GetPropertyOrDefault(
    const resource_coordinator::mojom::PropertyType property_type,
    int64_t default_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t value = 0;
  if (GetProperty(property_type, &value))
    return value;
  return default_value;
}

void NodeBase::OnEventReceived(resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnEventReceived(this, event);
}

void NodeBase::OnPropertyChanged(
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnPropertyChanged(this, property_type, value);
}

void NodeBase::SendEvent(resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnEventReceived(event);
}

void NodeBase::SetProperty(
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The |GraphObserver| API specification dictates that
  // the property is guaranteed to be set on the |NodeBase|
  // and propagated to the appropriate associated |CoordianationUnitBase|
  // before |OnPropertyChanged| is invoked on all of the registered observers.
  properties_[property_type] = value;
  OnPropertyChanged(property_type, value);
}

}  // namespace performance_manager
