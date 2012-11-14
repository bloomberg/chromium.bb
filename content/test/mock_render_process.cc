// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_render_process.h"

#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

namespace content {

MockRenderProcess::MockRenderProcess()
    : transport_dib_next_sequence_number_(0),
      enabled_bindings_(0) {
}

MockRenderProcess::~MockRenderProcess() {
}

skia::PlatformCanvas* MockRenderProcess::GetDrawingCanvas(
    TransportDIB** memory,
    const gfx::Rect& rect) {
  size_t stride = skia::PlatformCanvasStrideForWidth(rect.width());
  size_t size = stride * rect.height();

  // Unlike RenderProcessImpl, when we're a test, we can just create transport
  // DIBs in the current process, since there is no sandbox protecting us (and
  // no browser process to ask for one in any case).
  *memory = TransportDIB::Create(size, transport_dib_next_sequence_number_++);
  if (!*memory)
    return NULL;
  return (*memory)->GetPlatformCanvas(rect.width(), rect.height());
}

void MockRenderProcess::ReleaseTransportDIB(TransportDIB* memory) {
  delete memory;
}

bool MockRenderProcess::UseInProcessPlugins() const {
  return true;
}

void MockRenderProcess::AddBindings(int bindings) {
  enabled_bindings_ |= bindings;
}

int MockRenderProcess::GetEnabledBindings() const {
  return enabled_bindings_;
}

TransportDIB* MockRenderProcess::CreateTransportDIB(size_t size) {
  return TransportDIB::Create(size, transport_dib_next_sequence_number_++);
}

void MockRenderProcess::FreeTransportDIB(TransportDIB* dib) {
  delete dib;
}

}  // namespace content
