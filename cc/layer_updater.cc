// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer_updater.h"

namespace cc {

LayerUpdater::Texture::Texture(scoped_ptr<PrioritizedTexture> texture)
    : m_texture(texture.Pass())
{
}

LayerUpdater::Texture::~Texture()
{
}

}
