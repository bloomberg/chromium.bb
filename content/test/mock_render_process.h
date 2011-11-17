// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_PROCESS_H_
#define CONTENT_TEST_MOCK_RENDER_PROCESS_H_
#pragma once

#include "content/renderer/render_process.h"

// This class is a mock of the child process singleton which we use during
// running of the RenderView unit tests.
class MockRenderProcess : public RenderProcess {
 public:
  MockRenderProcess();
  virtual ~MockRenderProcess();

  // RenderProcess implementation.
  virtual skia::PlatformCanvas* GetDrawingCanvas(
      TransportDIB** memory,
      const gfx::Rect& rect) OVERRIDE;
  virtual void ReleaseTransportDIB(TransportDIB* memory) OVERRIDE;
  virtual bool UseInProcessPlugins() const OVERRIDE;
  virtual bool HasInitializedMediaLibrary() const;

 private:
  uint32 transport_dib_next_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcess);
};

#endif  // CONTENT_TEST_MOCK_RENDER_PROCESS_H_
