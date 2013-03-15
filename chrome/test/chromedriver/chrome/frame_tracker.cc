// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/frame_tracker.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

FrameTracker::FrameTracker(DevToolsClient* client) : client_(client) {
  DCHECK(client_);
  client_->AddListener(this);
}

FrameTracker::~FrameTracker() {}

Status FrameTracker::GetFrameForContextId(
    int context_id, std::string* frame_id) {
  if (context_to_frame_map_.count(context_id) == 0)
    return Status(kUnknownError, "execution context does not have a frame");
  *frame_id = context_to_frame_map_[context_id];
  return Status(kOk);
}

Status FrameTracker::GetContextIdForFrame(
    const std::string& frame_id, int* context_id) {
  if (frame_to_context_map_.count(frame_id) == 0)
    return Status(kUnknownError, "frame does not have execution context");
  *context_id = frame_to_context_map_[frame_id];
  return Status(kOk);
}

Status FrameTracker::OnConnected() {
  frame_to_context_map_.clear();
  context_to_frame_map_.clear();
  // Enable runtime events to allow tracking execution context creation.
  base::DictionaryValue params;
  Status status = client_->SendCommand("Runtime.enable", params);
  if (status.IsError())
    return status;
  return client_->SendCommand("DOM.getDocument", params);
}

void FrameTracker::OnEvent(const std::string& method,
                           const base::DictionaryValue& params) {
  if (method == "Runtime.executionContextCreated") {
    const base::DictionaryValue* context;
    if (!params.GetDictionary("context", &context)) {
      LOG(ERROR) << "Runtime.executionContextCreated missing dict 'context'";
      return;
    }
    int context_id;
    std::string frame_id;
    if (!context->GetInteger("id", &context_id) ||
        !context->GetString("frameId", &frame_id)) {
      std::string json;
      base::JSONWriter::Write(context, &json);
      LOG(ERROR) << "Runtime.executionContextCreated has invalid 'context': "
                 << json;
      return;
    }
    frame_to_context_map_.insert(std::make_pair(frame_id, context_id));
    context_to_frame_map_.insert(std::make_pair(context_id, frame_id));
  } else if (method == "DOM.documentUpdated") {
    frame_to_context_map_.clear();
    context_to_frame_map_.clear();
  }
}
