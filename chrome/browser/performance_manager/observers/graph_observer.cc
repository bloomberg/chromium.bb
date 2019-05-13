// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

GraphObserver::~GraphObserver() = default;

GraphObserverDefaultImpl::GraphObserverDefaultImpl() = default;

GraphObserverDefaultImpl::~GraphObserverDefaultImpl() = default;

void GraphObserverDefaultImpl::SetNodeGraph(GraphImpl* graph) {
  node_graph_ = graph;
}

}  // namespace performance_manager
