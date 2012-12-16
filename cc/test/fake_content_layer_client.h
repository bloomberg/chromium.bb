// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

#include "base/compiler_specific.h"
#include "cc/content_layer_client.h"

namespace cc {

class FakeContentLayerClient : public cc::ContentLayerClient {
public:
    FakeContentLayerClient();

    virtual void paintContents(SkCanvas*, const gfx::Rect& rect, gfx::RectF& opaqueRect) OVERRIDE;

    void setPaintAllOpaque(bool opaque) { m_paintAllOpaque = opaque; }

private:
    bool m_paintAllOpaque;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
