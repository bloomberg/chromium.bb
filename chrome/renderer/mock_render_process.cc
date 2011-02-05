// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_render_process.h"

#include "app/surface/transport_dib.h"
#include "ui/gfx/rect.h"

MockRenderProcess::MockRenderProcess()
    : transport_dib_next_sequence_number_(0) {
}

MockRenderProcess::~MockRenderProcess() {
}

skia::PlatformCanvas* MockRenderProcess::GetDrawingCanvas(
    TransportDIB** memory,
    const gfx::Rect& rect) {
  size_t stride = skia::PlatformCanvas::StrideForWidth(rect.width());
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

bool MockRenderProcess::HasInitializedMediaLibrary() const {
  return false;
}


