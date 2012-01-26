// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_render_view_observer.h"

namespace content {

ShellRenderViewObserver::ShellRenderViewObserver(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ShellRenderViewObserver::~ShellRenderViewObserver() {
}

bool ShellRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace content
