// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_STATS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_STATS_H_

#include <stdint.h>

#include <vector>

#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace resource_coordinator {

struct TabStats {
  TabStats();
  TabStats(const TabStats& other);
  TabStats(TabStats&& other) noexcept;
  ~TabStats();

  TabStats& operator=(const TabStats& other);

  bool is_app = false;            // Browser window is an app.
  bool is_internal_page = false;  // Internal page, such as NTP or Settings.
  // Playing audio, accessing cam/mic or mirroring display.
  bool is_media = false;
  bool is_pinned = false;
  bool is_in_visible_window = false;
  bool is_in_active_window = false;
  // Whether this is the active tab in a browser window.
  bool is_active = false;
  bool is_discarded = false;
  // User has entered text in a form.
  bool has_form_entry = false;
  int discard_count = 0;
  bool has_beforeunload_handler = false;
  base::TimeTicks last_active;
  base::TimeTicks last_hidden;
  content::RenderProcessHost* render_process_host = nullptr;
  base::ProcessHandle renderer_handle = 0;
  int child_process_host_id = 0;
  base::string16 title;
#if defined(OS_CHROMEOS)
  int oom_score = 0;
#endif
  int64_t tab_contents_id = 0;  // Unique ID per WebContents.
  bool is_auto_discardable = true;
};

typedef std::vector<TabStats> TabStatsList;

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_STATS_H_
