// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task.h"

#include "base/logging.h"

namespace cc {

Task::Task() : will_run_(false), did_run_(false) {}

Task::~Task() {
  DCHECK(!will_run_);
}

void Task::WillRun() {
  DCHECK(!will_run_);
  DCHECK(!did_run_);
  will_run_ = true;
}

void Task::DidRun() {
  DCHECK(will_run_);
  will_run_ = false;
  did_run_ = true;
}

bool Task::HasFinishedRunning() const {
  return did_run_;
}

TaskGraph::TaskGraph() {}

TaskGraph::TaskGraph(const TaskGraph& other) = default;

TaskGraph::~TaskGraph() {}

void TaskGraph::Swap(TaskGraph* other) {
  nodes.swap(other->nodes);
  edges.swap(other->edges);
}

void TaskGraph::Reset() {
  nodes.clear();
  edges.clear();
}

}  // namespace cc
