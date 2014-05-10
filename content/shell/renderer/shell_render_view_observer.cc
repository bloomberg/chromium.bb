// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_render_view_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

ShellRenderViewObserver::ShellRenderViewObserver(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

void ShellRenderViewObserver::DidClearWindowObject(
    blink::WebLocalFrame* frame) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::injectInternalsObject(frame);
  }
}

}  // namespace content
