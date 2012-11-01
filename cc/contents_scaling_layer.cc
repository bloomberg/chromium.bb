// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/contents_scaling_layer.h"

namespace cc {

ContentsScalingLayer::ContentsScalingLayer()
    : m_contentsScale(1.0) {
}

ContentsScalingLayer::~ContentsScalingLayer() {
}

IntSize ContentsScalingLayer::contentBounds() const {
  return IntSize(ceil(bounds().width() * contentsScaleX()),
                 ceil(bounds().height() * contentsScaleY()));
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
