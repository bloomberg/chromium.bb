// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_render_process_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_thread.h"
#include "content/shell/layout_test_controller_bindings.h"
#include "content/shell/shell_switches.h"

namespace content {

ShellRenderProcessObserver::ShellRenderProcessObserver() {
  RenderThread::Get()->AddObserver(this);
}

ShellRenderProcessObserver::~ShellRenderProcessObserver() {
}

void ShellRenderProcessObserver::WebKitInitialized() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    RenderThread::Get()->RegisterExtension(new LayoutTestControllerBindings());
}

}  // namespace content
