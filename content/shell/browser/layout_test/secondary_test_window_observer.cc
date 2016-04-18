// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/secondary_test_window_observer.h"

#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/common/shell_messages.h"

namespace content {

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SecondaryTestWindowObserver);

SecondaryTestWindowObserver::SecondaryTestWindowObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

SecondaryTestWindowObserver::~SecondaryTestWindowObserver() {}

bool SecondaryTestWindowObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SecondaryTestWindowObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SecondaryTestWindowObserver::OnTestFinishedInSecondaryRenderer() {
  BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

void SecondaryTestWindowObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK(!BlinkTestController::Get()->IsMainWindow(
      WebContents::FromRenderFrameHost(render_frame_host)));
  BlinkTestController::Get()->HandleNewRenderFrameHost(render_frame_host);
}

void SecondaryTestWindowObserver::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  DCHECK(!BlinkTestController::Get()->IsMainWindow(
      WebContents::FromRenderFrameHost(new_host)));
  BlinkTestController::Get()->HandleNewRenderFrameHost(new_host);
}

}  // namespace content
