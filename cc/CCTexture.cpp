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
    unsigned int componentsPerPixel = 4;
    unsigned int bytesPerComponent = 1;
    return componentsPerPixel * bytesPerComponent * size.width() * size.height();
}

}
