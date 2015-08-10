// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/replicated_frame_state.h"

#include "components/html_viewer/html_frame_properties.h"

namespace html_viewer {

ReplicatedFrameState::ReplicatedFrameState()
    : sandbox_flags(blink::WebSandboxFlags::None),
      tree_scope(blink::WebTreeScopeType::Document) {}

ReplicatedFrameState::~ReplicatedFrameState() {}

void SetReplicatedFrameStateFromClientProperties(
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties,
    ReplicatedFrameState* state) {
  state->name = FrameNameFromClientProperty(
      GetValueFromClientProperties(kPropertyFrameName, properties));
  state->origin = FrameOriginFromClientProperty(
      GetValueFromClientProperties(kPropertyFrameOrigin, properties));
  if (!FrameSandboxFlagsFromClientProperty(
          GetValueFromClientProperties(kPropertyFrameSandboxFlags, properties),
          &(state->sandbox_flags))) {
    state->sandbox_flags = blink::WebSandboxFlags::None;
  }
  if (!FrameTreeScopeFromClientProperty(
          GetValueFromClientProperties(kPropertyFrameTreeScope, properties),
          &(state->tree_scope))) {
    state->tree_scope = blink::WebTreeScopeType::Document;
  }
}

void ClientPropertiesFromReplicatedFrameState(
    const ReplicatedFrameState& state,
    mojo::Map<mojo::String, mojo::Array<uint8_t>>* properties) {
  AddToClientPropertiesIfValid(kPropertyFrameName,
                               FrameNameToClientProperty(state.name).Pass(),
                               properties);
  AddToClientPropertiesIfValid(
      kPropertyFrameTreeScope,
      FrameTreeScopeToClientProperty(state.tree_scope).Pass(), properties);
  AddToClientPropertiesIfValid(
      kPropertyFrameSandboxFlags,
      FrameSandboxFlagsToClientProperty(state.sandbox_flags).Pass(),
      properties);
}

}  // namespace html_viewer
