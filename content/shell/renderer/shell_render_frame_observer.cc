// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_render_frame_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_process_observer.h"
#include "content/shell/renderer/test_runner/web_test_interfaces.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

ShellRenderFrameObserver::ShellRenderFrameObserver(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  render_frame->GetWebFrame()->setPermissionClient(
      ShellRenderProcessObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner()
          ->GetWebPermissions());
}

}  // namespace content
