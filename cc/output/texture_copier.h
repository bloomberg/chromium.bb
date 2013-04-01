// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_TEXTURE_COPIER_H_
#define CC_OUTPUT_TEXTURE_COPIER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/program_binding.h"
#include "cc/output/shader.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class CC_EXPORT TextureCopier {
 public:
  struct Parameters {
    unsigned source_texture;
    unsigned dest_texture;
    gfx::Size size;
  };
  // Copy the base level contents of |source_texture| to |dest_texture|. Both
  // texture objects must be complete and have a base level of |size|
  // dimensions. The color formats do not need to match, but |dest_texture| must
  // have a renderable format.
  virtual void CopyTexture(Parameters parameters) = 0;
  virtual void Flush() = 0;

  virtual ~TextureCopier() {}
};

class CC_EXPORT AcceleratedTextureCopier : public TextureCopier {
 public:
  static scoped_ptr<AcceleratedTextureCopier> Create(
      WebKit::WebGraphicsContext3D* context,
      bool using_bind_uniforms,
      int highp_threshold_min) {
    return make_scoped_ptr(
        new AcceleratedTextureCopier(
            context, using_bind_uniforms, highp_threshold_min));
  }
  virtual ~AcceleratedTextureCopier();

  virtual void CopyTexture(Parameters parameters) OVERRIDE;
  virtual void Flush() OVERRIDE;

 protected:
  AcceleratedTextureCopier(WebKit::WebGraphicsContext3D* context,
                           bool using_bind_uniforms,
                           int highp_threshold_min);

 private:
  typedef ProgramBinding<VertexShaderPosTexIdentity, FragmentShaderRGBATex>
      BlitProgram;

  WebKit::WebGraphicsContext3D* context_;
  GLuint fbo_;
  GLuint position_buffer_;
  scoped_ptr<BlitProgram> blit_program_;
  scoped_ptr<BlitProgram> blit_program_highp_;
  bool using_bind_uniforms_;
  int highp_threshold_min_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedTextureCopier);
};

}  // namespace cc

#endif  // CC_OUTPUT_TEXTURE_COPIER_H_
