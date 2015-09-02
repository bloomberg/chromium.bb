// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"

#include "base/process/kill.h"

TabRendererData::TabRendererData()
    : network_state(NETWORK_STATE_NONE),
      loading(false),
      crashed_status(base::TERMINATION_STATUS_STILL_RUNNING),
      incognito(false),
      show_icon(true),
      pinned(false),
      blocked(false),
      app(false),
      media_state(TAB_MEDIA_STATE_NONE) {
}

TabRendererData::~TabRendererData() {}

bool TabRendererData::IsCrashed() const {
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if defined(OS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}

bool TabRendererData::Equals(const TabRendererData& data) {
  return
      favicon.BackedBySameObjectAs(data.favicon) &&
      network_state == data.network_state &&
      title == data.title &&
      url == data.url &&
      loading == data.loading &&
      crashed_status == data.crashed_status &&
      incognito == data.incognito &&
      show_icon == data.show_icon &&
      pinned == data.pinned &&
      blocked == data.blocked &&
      app == data.app &&
      media_state == data.media_state;
}
