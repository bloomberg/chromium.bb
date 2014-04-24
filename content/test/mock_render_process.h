// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_PROCESS_H_
#define CONTENT_TEST_MOCK_RENDER_PROCESS_H_

#include "content/renderer/render_process.h"

namespace content {
// This class is a mock of the child process singleton which we use during
// running of the RenderView unit tests.
class MockRenderProcess : public RenderProcess {
 public:
  MockRenderProcess();
  virtual ~MockRenderProcess();

  // RenderProcess implementation.
  virtual void AddBindings(int bindings) OVERRIDE;
  virtual int GetEnabledBindings() const OVERRIDE;

 private:
  int enabled_bindings_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcess);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_PROCESS_H_
