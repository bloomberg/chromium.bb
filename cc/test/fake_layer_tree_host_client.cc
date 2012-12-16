// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_client.h"

namespace cc {

scoped_ptr<OutputSurface> FakeLayerImplTreeHostClient::createOutputSurface()
{
    if (m_useSoftwareRendering) {
        if (m_useDelegatingRenderer)
            return FakeOutputSurface::CreateDelegatingSoftware(make_scoped_ptr(new FakeSoftwareOutputDevice).PassAs<SoftwareOutputDevice>()).PassAs<OutputSurface>();

        return FakeOutputSurface::CreateSoftware(make_scoped_ptr(new FakeSoftwareOutputDevice).PassAs<SoftwareOutputDevice>()).PassAs<OutputSurface>();
    }

    WebKit::WebGraphicsContext3D::Attributes attrs;
    if (m_useDelegatingRenderer)
        return FakeOutputSurface::CreateDelegating3d(CompositorFakeWebGraphicsContext3D::create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();

    return FakeOutputSurface::Create3d(CompositorFakeWebGraphicsContext3D::create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
}

scoped_ptr<InputHandler> FakeLayerImplTreeHostClient::createInputHandler()
{
    return scoped_ptr<InputHandler>();
}

scoped_ptr<FontAtlas> FakeLayerImplTreeHostClient::createFontAtlas()
{
    return scoped_ptr<FontAtlas>();
}

}  // namespace cc
