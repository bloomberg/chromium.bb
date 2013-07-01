// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <cmath>
#include <string>
#include <vector>

#include "content/public/test/unittest_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace {

class ContextTestBase : public testing::Test {
 public:
  virtual void SetUp() {
    WebKit::WebGraphicsContext3D::Attributes attributes;
    context_ = webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl::
        CreateOffscreenContext(attributes);
    context_->makeContextCurrent();
  }

  virtual void TearDown() {
    context_.reset(NULL);
  }

 protected:
  scoped_ptr<WebKit::WebGraphicsContext3D> context_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"
