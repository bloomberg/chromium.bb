// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ProgramBinding.h"

#include "CCRendererGL.h" // For the GLC() macro.
#include "GeometryBinding.h"
#include "GraphicsContext3D.h"
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>

using WebKit::WebGraphicsContext3D;

namespace cc {

ProgramBindingBase::ProgramBindingBase()
    : m_program(0)
    , m_vertexShaderId(0)
    , m_fragmentShaderId(0)
    , m_initialized(false)
{
}

ProgramBindingBase::~ProgramBindingBase()
{
    // If you hit these asserts, you initialized but forgot to call cleanup().
    ASSERT(!m_program);
    ASSERT(!m_vertexShaderId);
    ASSERT(!m_fragmentShaderId);
    ASSERT(!m_initialized);
}

static bool contextLost(WebGraphicsContext3D* context)
{
    return (context->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR);
}


void ProgramBindingBase::init(WebGraphicsContext3D* context, const std::string& vertexShader, const std::string& fragmentShader)
{
    TRACE_EVENT0("cc", "ProgramBindingBase::init");
    m_vertexShaderId = loadShader(context, GraphicsContext3D::VERTEX_SHADER, vertexShader);
    if (!m_vertexShaderId) {
        if (!contextLost(context))
            LOG_ERROR("Failed to create vertex shader");
        return;
    }

    m_fragmentShaderId = loadShader(context, GraphicsContext3D::FRAGMENT_SHADER, fragmentShader);
    if (!m_fragmentShaderId) {
        GLC(context, context->deleteShader(m_vertexShaderId));
        m_vertexShaderId = 0;
        if (!contextLost(context))
            LOG_ERROR("Failed to create fragment shader");
        return;
    }

    m_program = createShaderProgram(context, m_vertexShaderId, m_fragmentShaderId);
    ASSERT(m_program || contextLost(context));
}

void ProgramBindingBase::link(WebGraphicsContext3D* context)
{
    GLC(context, context->linkProgram(m_program));
    cleanupShaders(context);
#ifndef NDEBUG
    int linked = 0;
    GLC(context, context->getProgramiv(m_program, GraphicsContext3D::LINK_STATUS, &linked));
    if (!linked) {
        if (!contextLost(context))
            LOG_ERROR("Failed to link shader program");
        GLC(context, context->deleteProgram(m_program));
        return;
    }
#endif
}

void ProgramBindingBase::cleanup(WebGraphicsContext3D* context)
{
    m_initialized = false;
    if (!m_program)
        return;

    ASSERT(context);
    GLC(context, context->deleteProgram(m_program));
    m_program = 0;

    cleanupShaders(context);
}

unsigned ProgramBindingBase::loadShader(WebGraphicsContext3D* context, unsigned type, const std::string& shaderSource)
{
    unsigned shader = context->createShader(type);
    if (!shader)
        return 0;
    GLC(context, context->shaderSource(shader, shaderSource.data()));
    GLC(context, context->compileShader(shader));
#ifndef NDEBUG
    int compiled = 0;
    GLC(context, context->getShaderiv(shader, GraphicsContext3D::COMPILE_STATUS, &compiled));
    if (!compiled) {
        GLC(context, context->deleteShader(shader));
        return 0;
    }
#endif
    return shader;
}

unsigned ProgramBindingBase::createShaderProgram(WebGraphicsContext3D* context, unsigned vertexShader, unsigned fragmentShader)
{
    unsigned programObject = context->createProgram();
    if (!programObject) {
        if (!contextLost(context))
            LOG_ERROR("Failed to create shader program");
        return 0;
    }

    GLC(context, context->attachShader(programObject, vertexShader));
    GLC(context, context->attachShader(programObject, fragmentShader));

    // Bind the common attrib locations.
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::positionAttribLocation(), "a_position"));
    GLC(context, context->bindAttribLocation(programObject, GeometryBinding::texCoordAttribLocation(), "a_texCoord"));

    return programObject;
}

void ProgramBindingBase::cleanupShaders(WebGraphicsContext3D* context)
{
    if (m_vertexShaderId) {
        GLC(context, context->deleteShader(m_vertexShaderId));
        m_vertexShaderId = 0;
    }
    if (m_fragmentShaderId) {
        GLC(context, context->deleteShader(m_fragmentShaderId));
        m_fragmentShaderId = 0;
    }
}

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
