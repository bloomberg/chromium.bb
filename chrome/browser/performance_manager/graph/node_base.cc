// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/node_base.h"

#include <utility>

#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

NodeBase::NodeBase(NodeTypeEnum node_type, GraphImpl* graph)
    : graph_(graph), type_(node_type) {}

NodeBase::~NodeBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The node must have been removed from the graph before destruction.
  DCHECK(!graph_->NodeInGraph(this));
}

// static
int64_t NodeBase::GetSerializationId(NodeBase* node) {
  if (!node)
    return 0u;

  if (!node->serialization_id_)
    node->serialization_id_ = node->graph()->GetNextNodeSerializationId();

  DCHECK_NE(0u, node->serialization_id_);
  return node->serialization_id_;
}

void NodeBase::AddObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void NodeBase::RemoveObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void NodeBase::JoinGraph() {
  DCHECK(graph_->NodeInGraph(this));
}

void NodeBase::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_->NodeInGraph(this));

  for (auto& observer : observers_)
    observer.OnBeforeNodeRemoved(this);
}

}  // namespace performance_manager
