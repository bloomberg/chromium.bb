// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_COMPOSITOR_TEST_BLIMP_EMBEDDER_COMPOSITOR_H_
#define BLIMP_CLIENT_TEST_COMPOSITOR_TEST_BLIMP_EMBEDDER_COMPOSITOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/support/compositor/blimp_embedder_compositor.h"

namespace blimp {
namespace client {
class CompositorDependencies;

// An implementation of the BlimpEmbedderCompositor that supplies a
// ContextProvider that is not backed by a platform specific widget.  It is not
// meant to be used to actually display content.
class TestBlimpEmbedderCompositor : public BlimpEmbedderCompositor {
 public:
  explicit TestBlimpEmbedderCompositor(
      CompositorDependencies* compositor_dependencies);
  ~TestBlimpEmbedderCompositor() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBlimpEmbedderCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_COMPOSITOR_TEST_BLIMP_EMBEDDER_COMPOSITOR_H_
