// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_STATS_H_
#define CHROME_BROWSER_MEMORY_TAB_STATS_H_

#include <stdint.h>

#include <vector>

#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace memory {

struct TabStats {
  TabStats();
  TabStats(const TabStats& other);
  ~TabStats();
  bool is_app;            // Browser window is an app.
  bool is_internal_page;  // Internal page, such as NTP or Settings.
  bool is_media;  // Playing audio, acessing cam/mic or mirroring display.
  bool is_pinned;
  bool is_selected;  // Selected in the currently active browser window.
  bool is_discarded;
  bool has_form_entry;  // User has entered text in a form.
  int discard_count;
  base::TimeTicks last_active;
  base::TimeTicks last_hidden;
  content::RenderProcessHost* render_process_host;
  base::ProcessHandle renderer_handle;
  int child_process_host_id;
  base::string16 title;
#if defined(OS_CHROMEOS)
  int oom_score;
#endif
  int64_t tab_contents_id;  // Unique ID per WebContents.
  bool is_auto_discardable;
};

typedef std::vector<TabStats> TabStatsList;

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_STATS_H_
