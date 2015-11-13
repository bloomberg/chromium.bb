// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_REPLICATED_FRAME_STATE_H_
#define COMPONENTS_HTML_VIEWER_REPLICATED_FRAME_STATE_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "url/origin.h"

namespace html_viewer {

// Stores state shared with other frames in the tree.
struct ReplicatedFrameState {
 public:
  ReplicatedFrameState();
  ~ReplicatedFrameState();

  blink::WebString name;
  url::Origin origin;
  blink::WebSandboxFlags sandbox_flags;
  blink::WebTreeScopeType tree_scope;
};

// Sets |state| from |properties|.
void SetReplicatedFrameStateFromClientProperties(
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties,
    ReplicatedFrameState* state);

// Sets |properties| from |state|.
void ClientPropertiesFromReplicatedFrameState(
    const ReplicatedFrameState& state,
    mojo::Map<mojo::String, mojo::Array<uint8_t>>* properties);

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_REPLICATED_FRAME_STATE_H_
