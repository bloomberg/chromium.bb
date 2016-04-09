// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/traced_frame_tree_node.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

TracedFrameTreeNode::TracedFrameTreeNode(const FrameTreeNode& node)
    : parent_node_id_(-1),
      process_id_(-1),
      routing_id_(-1) {
  FrameTreeNode* parent = node.parent();
  if (parent)
    parent_node_id_ = parent->frame_tree_node_id();

  RenderFrameHostImpl* current_frame_host = node.current_frame_host();

  if (current_frame_host->last_committed_url().is_valid())
    url_ = current_frame_host->last_committed_url().spec();

  // On Windows, |rph->GetHandle()| does not duplicate ownership
  // of the process handle and the render host still retains it. Therefore, we
  // cannot create a base::Process object, which provides a proper way to get a
  // process id, from the handle. For a stopgap, we use this deprecated
  // function that does not require the ownership (http://crbug.com/417532).
  process_id_ = base::GetProcId(current_frame_host->GetProcess()->GetHandle());

  routing_id_ = current_frame_host->GetRoutingID();
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
}

TracedFrameTreeNode::~TracedFrameTreeNode() {
}

void TracedFrameTreeNode::AppendAsTraceFormat(std::string* out) const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  if (parent_node_id_ >= 0) {
    std::unique_ptr<base::DictionaryValue> ref(new base::DictionaryValue());
    ref->SetString("id_ref", base::StringPrintf("0x%x", parent_node_id_));
    ref->SetString("scope", "FrameTreeNode");
    value->Set("parent", std::move(ref));
  }

  if (process_id_ >= 0) {
    std::unique_ptr<base::DictionaryValue> ref(new base::DictionaryValue());
    ref->SetInteger("pid_ref", process_id_);
    ref->SetString("id_ref", base::StringPrintf("0x%x", routing_id_));
    ref->SetString("scope", "RenderFrame");
    value->Set("RenderFrame", std::move(ref));
  }

  if (!url_.empty())
    value->SetString("url", url_);

  std::string tmp;
  base::JSONWriter::Write(*value, &tmp);
  *out += tmp;
}

}  // content
