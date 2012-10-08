// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "LayerTextureUpdater.h"

namespace cc {

LayerTextureUpdater::Texture::Texture(PassOwnPtr<CCPrioritizedTexture> texture)
    : m_texture(texture)
{
}

LayerTextureUpdater::Texture::~Texture()
{
}

bool LayerTextureUpdater::Texture::backingResourceWasEvicted() const
{
    return m_texture->backingResourceWasEvicted();
}

}
