// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/frame_tracker.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

FrameTracker::FrameTracker(DevToolsClient* client) {
  client->AddListener(this);
}

FrameTracker::~FrameTracker() {}

Status FrameTracker::GetContextIdForFrame(
    const std::string& frame_id, int* context_id) {
  if (frame_to_context_map_.count(frame_id) == 0) {
    return Status(kNoSuchExecutionContext,
                  "frame does not have execution context");
  }
  *context_id = frame_to_context_map_[frame_id];
  return Status(kOk);
}

Status FrameTracker::OnConnected(DevToolsClient* client) {
  frame_to_context_map_.clear();
  // Enable runtime events to allow tracking execution context creation.
  base::DictionaryValue params;
  Status status = client->SendCommand("Runtime.enable", params);
  if (status.IsError())
    return status;
  return client->SendCommand("Page.enable", params);
}

Status FrameTracker::OnEvent(DevToolsClient* client,
                             const std::string& method,
                             const base::DictionaryValue& params) {
  if (method == "Runtime.executionContextCreated") {
    const base::DictionaryValue* context;
    if (!params.GetDictionary("context", &context)) {
      return Status(kUnknownError,
                    "Runtime.executionContextCreated missing dict 'context'");
    }

    int context_id;
    std::string frame_id;
    if (!context->GetInteger("id", &context_id) ||
        !context->GetString("frameId", &frame_id)) {
      std::string json;
      base::JSONWriter::Write(*context, &json);
      return Status(kUnknownError, method + " has invalid 'context': " + json);
    }
    std::string type;
    if (context->HasKey("type") && !context->GetString("type", &type))
      return Status(kUnknownError, method + " has invalid 'context.type'");
    if (type != "Extension")  // exclude content scripts
      frame_to_context_map_[frame_id] = context_id;
  } else if (method == "Runtime.executionContextDestroyed") {
    int execution_context_id;
    if (!params.GetInteger("executionContextId", &execution_context_id))
      return Status(kUnknownError, method + " missing 'executionContextId'");
    for (auto entry : frame_to_context_map_) {
      if (entry.second == execution_context_id) {
        frame_to_context_map_.erase(entry.first);
        break;
      }
    }
  } else if (method == "Runtime.executionContextsCleared") {
    frame_to_context_map_.clear();
  } else if (method == "Page.frameNavigated") {
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value))
      frame_to_context_map_.clear();
  }
  return Status(kOk);
}
