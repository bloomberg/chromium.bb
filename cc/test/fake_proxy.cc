// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_proxy.h"

namespace cc {

bool FakeProxy::CompositeAndReadback(void* pixels, gfx::Rect rect)
{
    return true;
}

bool FakeProxy::IsStarted() const
{
    return true;
}

bool FakeProxy::InitializeOutputSurface()
{
    return true;
}

bool FakeProxy::InitializeRenderer()
{
    return true;
}

bool FakeProxy::RecreateOutputSurface()
{
    return true;
}

const RendererCapabilities& FakeProxy::GetRendererCapabilities() const
{
    return m_capabilities;
}

RendererCapabilities& FakeProxy::GetRendererCapabilities()
{
    return m_capabilities;
}

bool FakeProxy::CommitRequested() const
{
    return false;
}

size_t FakeProxy::MaxPartialTextureUpdates() const
{
    return m_maxPartialTextureUpdates;
}

void FakeProxy::setMaxPartialTextureUpdates(size_t max)
{
    m_maxPartialTextureUpdates = max;
}

bool FakeProxy::CommitPendingForTesting()
{
    return false;
}

skia::RefPtr<SkPicture> FakeProxy::CapturePicture()
{
    return skia::RefPtr<SkPicture>();
}

scoped_ptr<base::Value> FakeProxy::AsValue() const {
    scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
    return state.PassAs<base::Value>();
}


}  // namespace cc
