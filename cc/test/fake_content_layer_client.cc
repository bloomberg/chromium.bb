// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include "ui/gfx/rect.h"

namespace cc {

FakeContentLayerClient::FakeContentLayerClient()
    : m_paintAllOpaque(false)
{
}

void FakeContentLayerClient::paintContents(SkCanvas*, const gfx::Rect& rect, gfx::RectF& opaqueRect)
{
  if (m_paintAllOpaque)
    opaqueRect = rect;
}

}  // namespace cc
