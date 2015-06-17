// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace skia_runner {

class InProcessGraphicsSystem {
 public:
  InProcessGraphicsSystem();
  ~InProcessGraphicsSystem();

  bool IsSuccessfullyInitialized() const;
  skia::RefPtr<GrContext> GetGrContext() const { return gr_context_; }
  int GetMaxTextureSize() const;

 private:
  scoped_ptr<gpu::GLInProcessContext> CreateInProcessContext() const;

  scoped_ptr<gpu::GLInProcessContext> in_process_context_;
  skia::RefPtr<GrContext> gr_context_;
};

}  // namespace skia_runner
