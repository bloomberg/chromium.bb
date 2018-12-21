// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/stub_render_widget_host_owner_delegate.h"

namespace content {

bool StubRenderWidgetHostOwnerDelegate::MayRenderWidgetForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  return true;
}

bool StubRenderWidgetHostOwnerDelegate::ShouldContributePriorityToProcess() {
  return false;
}

RenderViewHost* StubRenderWidgetHostOwnerDelegate::GetRenderViewHost() {
  // TODO(danakj): This could make a StubRenderViewHost and return that if
  // needed.
  return nullptr;
}

}  // namespace content
