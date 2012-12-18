// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_copier.h"

#include "base/debug/trace_event.h"
#include "build/build_config.h"
#include "cc/gl_renderer.h" // For the GLC() macro.
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

AcceleratedTextureCopier::AcceleratedTextureCopier(WebKit::WebGraphicsContext3D* context, bool usingBindUniforms)
    : m_context(context)
    , m_usingBindUniforms(usingBindUniforms)
{
    DCHECK(m_context);
    GLC(m_context, m_fbo = m_context->createFramebuffer());
    GLC(m_context, m_positionBuffer = m_context->createBuffer());

    static const float kPositions[4][4] = {
        {-1, -1, 0, 1},
        { 1, -1, 0, 1},
        { 1, 1, 0, 1},
        {-1, 1, 0, 1}
    };

    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, m_positionBuffer));
    GLC(m_context, m_context->bufferData(GL_ARRAY_BUFFER, sizeof(kPositions), kPositions, GL_STATIC_DRAW));
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, 0));

    m_blitProgram.reset(new BlitProgram(m_context));
}

AcceleratedTextureCopier::~AcceleratedTextureCopier()
{
    if (m_blitProgram)
        m_blitProgram->cleanup(m_context);
    if (m_positionBuffer)
        GLC(m_context, m_context->deleteBuffer(m_positionBuffer));
    if (m_fbo)
        GLC(m_context, m_context->deleteFramebuffer(m_fbo));
}

void AcceleratedTextureCopier::copyTexture(Parameters parameters)
{
    TRACE_EVENT0("cc", "TextureCopier::copyTexture");

    GLC(m_context, m_context->disable(GL_SCISSOR_TEST));

    // Note: this code does not restore the viewport, bound program, 2D texture, framebuffer, buffer or blend enable.
    GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    GLC(m_context, m_context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, parameters.destTexture, 0));

#if defined(OS_ANDROID)
    // Clear destination to improve performance on tiling GPUs.
    // TODO: Use EXT_discard_framebuffer or skip clearing if it isn't available.
    GLC(m_context, m_context->clear(GL_COLOR_BUFFER_BIT));
#endif

    GLC(m_context, m_context->bindTexture(GL_TEXTURE_2D, parameters.sourceTexture));
    GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    if (!m_blitProgram->initialized())
        m_blitProgram->initialize(m_context, m_usingBindUniforms);

    // TODO: Use EXT_framebuffer_blit if available.
    GLC(m_context, m_context->useProgram(m_blitProgram->program()));

    const int kPositionAttribute = 0;
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, m_positionBuffer));
    GLC(m_context, m_context->vertexAttribPointer(kPositionAttribute, 4, GL_FLOAT, false, 0, 0));
    GLC(m_context, m_context->enableVertexAttribArray(kPositionAttribute));
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, 0));

    GLC(m_context, m_context->viewport(0, 0, parameters.size.width(), parameters.size.height()));
    GLC(m_context, m_context->disable(GL_BLEND));
    GLC(m_context, m_context->drawArrays(GL_TRIANGLE_FAN, 0, 4));

    GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(m_context, m_context->disableVertexAttribArray(kPositionAttribute));

    GLC(m_context, m_context->useProgram(0));

    GLC(m_context, m_context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0));
    GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, 0));
    GLC(m_context, m_context->bindTexture(GL_TEXTURE_2D, 0));

    GLC(m_context, m_context->enable(GL_SCISSOR_TEST));
}

void AcceleratedTextureCopier::flush()
{
    GLC(m_context, m_context->flush());
}

}  // namespace cc
