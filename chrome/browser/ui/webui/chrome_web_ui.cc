// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace chrome_web_ui {

// If true, overrides IsMoreWebUI flag.
static bool g_override_more_webui = false;

bool IsMoreWebUI() {
  bool more_webui = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMoreWebUI) || g_override_more_webui;
#if defined(TOOLKIT_VIEWS)
  more_webui |= views::Widget::IsPureViews();
#endif
  return more_webui;
}

void OverrideMoreWebUI(bool use_more_webui) {
  g_override_more_webui = use_more_webui;
}

}  // namespace chrome_web_ui
