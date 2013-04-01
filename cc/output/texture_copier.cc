// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/texture_copier.h"

#include "base/debug/trace_event.h"
#include "build/build_config.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

AcceleratedTextureCopier::AcceleratedTextureCopier(
    WebKit::WebGraphicsContext3D* context,
    bool using_bind_uniforms,
    int highp_threshold_min)
    : context_(context)
    , using_bind_uniforms_(using_bind_uniforms)
    , highp_threshold_min_(highp_threshold_min) {
  DCHECK(context_);
  GLC(context_, fbo_ = context_->createFramebuffer());
  GLC(context_, position_buffer_ = context_->createBuffer());

  static const float kPositions[4][4] = { { -1, -1, 0, 1 }, { 1, -1, 0, 1 },
                                          { 1, 1, 0, 1 }, { -1, 1, 0, 1 } };

  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, position_buffer_));
  GLC(context_,
      context_->bufferData(
          GL_ARRAY_BUFFER, sizeof(kPositions), kPositions, GL_STATIC_DRAW));
  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, 0));

  blit_program_.reset(new BlitProgram(context_, TexCoordPrecisionMedium));
  blit_program_highp_.reset(new BlitProgram(context_, TexCoordPrecisionHigh));
}

AcceleratedTextureCopier::~AcceleratedTextureCopier() {
  if (blit_program_)
    blit_program_->Cleanup(context_);
  if (blit_program_highp_)
    blit_program_highp_->Cleanup(context_);
  if (position_buffer_)
    GLC(context_, context_->deleteBuffer(position_buffer_));
  if (fbo_)
    GLC(context_, context_->deleteFramebuffer(fbo_));
}

void AcceleratedTextureCopier::CopyTexture(Parameters parameters) {
  TRACE_EVENT0("cc", "TextureCopier::CopyTexture");

  GLC(context_, context_->disable(GL_SCISSOR_TEST));

  // Note: this code does not restore the viewport, bound program, 2D texture,
  // framebuffer, buffer or blend enable.
  GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, fbo_));
  GLC(context_,
      context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                     GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D,
                                     parameters.dest_texture,
                                     0));

#if defined(OS_ANDROID)
  // Clear destination to improve performance on tiling GPUs.
  // TODO(epenner): Use EXT_discard_framebuffer or skip clearing if it isn't
  // available.
  GLC(context_, context_->clear(GL_COLOR_BUFFER_BIT));
#endif

  GLC(context_,
      context_->bindTexture(GL_TEXTURE_2D, parameters.source_texture));
  GLC(context_,
      context_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GLC(context_,
      context_->texParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

  TexCoordPrecision texCoordPrecision = TexCoordPrecisionRequired(
      context_, highp_threshold_min_, parameters.size);
  if (texCoordPrecision == TexCoordPrecisionHigh) {
    if (!blit_program_highp_->initialized())
      blit_program_highp_->Initialize(context_, using_bind_uniforms_);

    // TODO(danakj): Use EXT_framebuffer_blit if available.
    GLC(context_, context_->useProgram(blit_program_highp_->program()));
  } else {
    if (!blit_program_->initialized())
      blit_program_->Initialize(context_, using_bind_uniforms_);

    // TODO(danakj: Use EXT_framebuffer_blit if available.
    GLC(context_, context_->useProgram(blit_program_->program()));
  }

  const int kPositionAttribute = 0;
  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, position_buffer_));
  GLC(context_,
      context_->vertexAttribPointer(
          kPositionAttribute, 4, GL_FLOAT, false, 0, 0));
  GLC(context_, context_->enableVertexAttribArray(kPositionAttribute));
  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, 0));

  GLC(context_,
      context_->viewport(
          0, 0, parameters.size.width(), parameters.size.height()));
  GLC(context_, context_->disable(GL_BLEND));
  GLC(context_, context_->drawArrays(GL_TRIANGLE_FAN, 0, 4));

  GLC(context_,
      context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(context_,
      context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(context_, context_->disableVertexAttribArray(kPositionAttribute));

  GLC(context_, context_->useProgram(0));

  GLC(context_,
      context_->framebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
  GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, 0));
  GLC(context_, context_->bindTexture(GL_TEXTURE_2D, 0));

  GLC(context_, context_->enable(GL_SCISSOR_TEST));
}

void AcceleratedTextureCopier::Flush() {
  GLC(context_, context_->flush());
}

}  // namespace cc
