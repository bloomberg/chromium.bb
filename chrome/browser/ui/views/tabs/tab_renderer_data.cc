// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"

TabRendererData::TabRendererData()
    : network_state(NETWORK_STATE_NONE),
      common_prefix_length(0),
      loading(false),
      crashed_status(base::TERMINATION_STATUS_STILL_RUNNING),
      off_the_record(false),
      show_icon(true),
      mini(false),
      blocked(false),
      app(false) {
}

TabRendererData::~TabRendererData() {}

bool TabRendererData::Equals(const TabRendererData& data) {
  return
      favicon.pixelRef() &&
      favicon.pixelRef() == data.favicon.pixelRef() &&
      favicon.pixelRefOffset() == data.favicon.pixelRefOffset() &&
      network_state == data.network_state &&
      title == data.title &&
      common_prefix_length == data.common_prefix_length &&
      loading == data.loading &&
      crashed_status == data.crashed_status &&
      off_the_record == data.off_the_record &&
      show_icon == data.show_icon &&
      mini == data.mini &&
      blocked == data.blocked &&
      app == data.app;
}
