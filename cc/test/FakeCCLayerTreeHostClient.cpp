// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "FakeCCLayerTreeHostClient.h"

namespace cc {

scoped_ptr<WebKit::WebCompositorOutputSurface> FakeCCLayerTreeHostClient::createOutputSurface()
{
    WebKit::WebGraphicsContext3D::Attributes attrs;
    return scoped_ptr<WebKit::WebCompositorOutputSurface>(WebKit::FakeWebCompositorOutputSurface::create(WebKit::CompositorFakeWebGraphicsContext3D::create(attrs)));
}

scoped_ptr<CCInputHandler> FakeCCLayerTreeHostClient::createInputHandler()
{
    return scoped_ptr<CCInputHandler>();
}

}
