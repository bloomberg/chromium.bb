// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BASE_COMPOSITOR_DELEGATE_H_
#define CHROME_BROWSER_VR_BASE_COMPOSITOR_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/compositor_delegate.h"

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;
}  // namespace gl

namespace vr {

class VR_EXPORT BaseCompositorDelegate : public CompositorDelegate {
 public:
  BaseCompositorDelegate();
  ~BaseCompositorDelegate() override;

  // CompositorDelegate implementation.
  bool Initialize(const scoped_refptr<gl::GLSurface>& surface) override;
  bool RunInSkiaContext(SkiaContextCallback callback) override;

 private:
  enum ContextId { kNone = -1, kMainContext, kSkiaContext, kNumContexts };

  bool MakeContextCurrent(ContextId context_id);

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> contexts_[kNumContexts];
  ContextId curr_context_id_ = kNone;

  DISALLOW_COPY_AND_ASSIGN(BaseCompositorDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BASE_COMPOSITOR_DELEGATE_H_
