// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/memory/tab_stats.h"

namespace memory {

TabStats::TabStats()
    : is_app(false),
      is_internal_page(false),
      is_media(false),
      is_pinned(false),
      is_selected(false),
      is_discarded(false),
      has_form_entry(false),
      discard_count(0),
      render_process_host(nullptr),
      renderer_handle(0),
      child_process_host_id(0),
#if defined(OS_CHROMEOS)
      oom_score(0),
#endif
      tab_contents_id(0) {
}

TabStats::~TabStats() {
}

}  // namespace memory
