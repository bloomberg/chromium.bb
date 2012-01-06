// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

using content::WebContents;

namespace {

// If true, overrides IsMoreWebUI flag.
bool override_more_webui_ = false;

}  // namespace

ChromeWebUI::ChromeWebUI(WebContents* contents)
    : WebUI(contents) {
}

ChromeWebUI::~ChromeWebUI() {
}

// static
bool ChromeWebUI::IsMoreWebUI() {
  bool more_webui = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMoreWebUI) || override_more_webui_;
#if defined(TOOLKIT_VIEWS)
  more_webui |= views::Widget::IsPureViews();
#endif
  return more_webui;
}

void ChromeWebUI::OverrideMoreWebUI(bool use_more_webui) {
  override_more_webui_ = use_more_webui;
}
