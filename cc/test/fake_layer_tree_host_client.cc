// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_client.h"

#include "cc/context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

FakeLayerImplTreeHostClient::FakeLayerImplTreeHostClient(bool useSoftwareRendering, bool useDelegatingRenderer)
    : m_useSoftwareRendering(useSoftwareRendering)
    , m_useDelegatingRenderer(useDelegatingRenderer)
{
}

FakeLayerImplTreeHostClient::~FakeLayerImplTreeHostClient() { }

scoped_ptr<OutputSurface> FakeLayerImplTreeHostClient::createOutputSurface()
{
    if (m_useSoftwareRendering) {
        if (m_useDelegatingRenderer)
            return FakeOutputSurface::CreateDelegatingSoftware(make_scoped_ptr(new FakeSoftwareOutputDevice).PassAs<SoftwareOutputDevice>()).PassAs<OutputSurface>();

        return FakeOutputSurface::CreateSoftware(make_scoped_ptr(new FakeSoftwareOutputDevice).PassAs<SoftwareOutputDevice>()).PassAs<OutputSurface>();
    }

    WebKit::WebGraphicsContext3D::Attributes attrs;
    if (m_useDelegatingRenderer)
        return FakeOutputSurface::CreateDelegating3d(TestWebGraphicsContext3D::Create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();

    return FakeOutputSurface::Create3d(TestWebGraphicsContext3D::Create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<OutputSurface>();
}

scoped_ptr<InputHandler> FakeLayerImplTreeHostClient::createInputHandler()
{
    return scoped_ptr<InputHandler>();
}

scoped_refptr<cc::ContextProvider> FakeLayerImplTreeHostClient::OffscreenContextProviderForMainThread() {
    if (!m_mainThreadContexts || m_mainThreadContexts->DestroyedOnMainThread())
        m_mainThreadContexts = new FakeContextProvider;
    return m_mainThreadContexts;
}

scoped_refptr<cc::ContextProvider> FakeLayerImplTreeHostClient::OffscreenContextProviderForCompositorThread() {
    if (!m_compositorThreadContexts || m_compositorThreadContexts->DestroyedOnMainThread())
        m_compositorThreadContexts = new FakeContextProvider;
    return m_compositorThreadContexts;
}

}  // namespace cc
