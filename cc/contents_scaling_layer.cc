// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/contents_scaling_layer.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

ContentsScalingLayer::ContentsScalingLayer()
    : m_contentsScale(1.0) {
}

ContentsScalingLayer::~ContentsScalingLayer() {
}

gfx::Size ContentsScalingLayer::contentBounds() const {
  return gfx::ToCeiledSize(bounds().Scale(contentsScaleX(), contentsScaleY()));
}

float ContentsScalingLayer::contentsScaleX() const {
  return m_contentsScale;
}

float ContentsScalingLayer::contentsScaleY() const {
  return m_contentsScale;
}

void ContentsScalingLayer::setContentsScale(float contentsScale) {
  if (m_contentsScale == contentsScale)
    return;
  m_contentsScale = contentsScale;
  setNeedsDisplay();
}

}  // namespace cc
