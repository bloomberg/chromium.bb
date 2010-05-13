// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/base_tab_renderer.h"

BaseTabRenderer::BaseTabRenderer(TabController* controller)
    : controller_(controller) {
}

void BaseTabRenderer::SetData(const TabRendererData& data) {
  TabRendererData old(data_);
  data_ = data;
  DataChanged(old);
  Layout();
}

void BaseTabRenderer::UpdateLoadingAnimation(
    TabRendererData::NetworkState state) {
  if (state == data_.network_state &&
      state == TabRendererData::NETWORK_STATE_NONE) {
    // If the network state is none and hasn't changed, do nothing. Otherwise we
    // need to advance the animation frame.
    return;
  }

  data_.network_state = state;
  AdvanceLoadingAnimation(state);
}
