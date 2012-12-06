// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_client.h"

namespace cc {

scoped_ptr<WebKit::WebCompositorOutputSurface> FakeLayerImplTreeHostClient::createOutputSurface()
{
    if (m_useSoftwareRendering) {
        return WebKit::FakeWebCompositorOutputSurface::createSoftware(make_scoped_ptr(new WebKit::FakeWebCompositorSoftwareOutputDevice).PassAs<WebKit::WebCompositorSoftwareOutputDevice>()).PassAs<WebKit::WebCompositorOutputSurface>();
    }

    WebKit::WebGraphicsContext3D::Attributes attrs;
    return WebKit::FakeWebCompositorOutputSurface::create(WebKit::CompositorFakeWebGraphicsContext3D::create(attrs).PassAs<WebKit::WebGraphicsContext3D>()).PassAs<WebKit::WebCompositorOutputSurface>();
}

scoped_ptr<InputHandler> FakeLayerImplTreeHostClient::createInputHandler()
{
    return scoped_ptr<InputHandler>();
}

scoped_ptr<FontAtlas> FakeLayerImplTreeHostClient::createFontAtlas()
{
	return scoped_ptr<FontAtlas>();
}

}
