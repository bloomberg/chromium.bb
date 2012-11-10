// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

namespace cc {

bool FakeProxy::compositeAndReadback(void *pixels, const gfx::Rect&)
{
    return false;
}

bool FakeProxy::isStarted() const
{
    return false;
}

bool FakeProxy::initializeContext()
{
    return false;
}

bool FakeProxy::initializeRenderer()
{
    return false;
}

bool FakeProxy::recreateContext()
{
    return false;
}

const RendererCapabilities& FakeProxy::rendererCapabilities() const
{
    return m_capabilities;
}

bool FakeProxy::commitRequested() const
{
    return false;
}

size_t FakeProxy::maxPartialTextureUpdates() const
{
    return 0;
}


} // namespace cc
