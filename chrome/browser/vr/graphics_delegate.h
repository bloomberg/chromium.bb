// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/vr_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;
}  // namespace gl

namespace vr {

class VR_EXPORT GraphicsDelegate {
 public:
  explicit GraphicsDelegate(const scoped_refptr<gl::GLSurface>& surface);
  ~GraphicsDelegate();
  bool Initialize();
  bool MakeMainContextCurrent();
  bool MakeSkiaContextCurrent();

 private:
  enum ContextId { NONE = -1, MAIN_CONTEXT, SKIA_CONTEXT, NUM_CONTEXTS };

  bool MakeContextCurrent(ContextId context_id);

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> contexts_[NUM_CONTEXTS];
  ContextId curr_context_id_ = NONE;

  DISALLOW_COPY_AND_ASSIGN(GraphicsDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
