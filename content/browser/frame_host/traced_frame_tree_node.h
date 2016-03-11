// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

class FrameTree;
class FrameTreeNode;

// This is a temporary container used when tracing snapshots of FrameTree
// objects. When a snapshot of a FrameTree is taken, a TracedFrameTreeNode is
// created and stored by the tracing system until the trace is dumped.
class CONTENT_EXPORT TracedFrameTreeNode :
  public base::trace_event::ConvertableToTraceFormat {
 public:
  TracedFrameTreeNode(const FrameTreeNode& node);
  void AppendAsTraceFormat(std::string* out) const override;

 private:
  ~TracedFrameTreeNode() override;

  int parent_node_id_;
  std::string url_;
  int process_id_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(TracedFrameTreeNode);
};

}  // content
