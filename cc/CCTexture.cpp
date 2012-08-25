// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTexture.h"

namespace WebCore {

void CCTexture::setDimensions(const IntSize& size, GC3Denum format)
{
    m_size = size;
    m_format = format;
}

size_t CCTexture::bytes() const
{
    if (m_size.isEmpty())
        return 0u;

    return memorySizeBytes(m_size, m_format);
}

size_t CCTexture::memorySizeBytes(const IntSize& size, GC3Denum format)
{
    unsigned int componentsPerPixel;
    unsigned int bytesPerComponent;
    if (!GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent)) {
        ASSERT_NOT_REACHED();
        return 0u;
    }
    return componentsPerPixel * bytesPerComponent * size.width() * size.height();
}

}
