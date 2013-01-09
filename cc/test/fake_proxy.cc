// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

namespace cc {

bool FakeProxy::compositeAndReadback(void *pixels, const gfx::Rect&)
{
    return true;
}

bool FakeProxy::isStarted() const
{
    return true;
}

bool FakeProxy::initializeOutputSurface()
{
    return true;
}

bool FakeProxy::initializeRenderer()
{
    return true;
}

bool FakeProxy::recreateOutputSurface()
{
    return true;
}

const RendererCapabilities& FakeProxy::rendererCapabilities() const
{
    return m_capabilities;
}

RendererCapabilities& FakeProxy::rendererCapabilities()
{
    return m_capabilities;
}

bool FakeProxy::commitRequested() const
{
    return false;
}

size_t FakeProxy::maxPartialTextureUpdates() const
{
    return m_maxPartialTextureUpdates;
}

void FakeProxy::setMaxPartialTextureUpdates(size_t max)
{
    m_maxPartialTextureUpdates = max;
}

bool FakeProxy::commitPendingForTesting()
{
    return false;
}

skia::RefPtr<SkPicture> FakeProxy::capturePicture()
{
    return skia::RefPtr<SkPicture>();
}

}  // namespace cc
