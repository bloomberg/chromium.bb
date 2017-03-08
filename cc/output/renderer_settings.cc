// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include <limits>

#include "base/logging.h"
#include "cc/resources/platform_color.h"

namespace cc {

RendererSettings::RendererSettings()
    : preferred_tile_format(PlatformColor::BestTextureFormat()) {}

RendererSettings::RendererSettings(const RendererSettings& other) = default;

RendererSettings::~RendererSettings() {
}

}  // namespace cc
