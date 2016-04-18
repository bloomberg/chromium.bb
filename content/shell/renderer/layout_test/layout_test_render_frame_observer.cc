// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_render_frame_observer.h"

#include <string>

#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
#include "content/public/renderer/render_frame.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/layout_test/blink_test_runner.h"
#include "content/shell/renderer/layout_test/layout_test_render_thread_observer.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

LayoutTestRenderFrameObserver::LayoutTestRenderFrameObserver(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->setContentSettingsClient(
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner()
          ->GetWebContentSettings());
}

bool LayoutTestRenderFrameObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_LayoutDumpRequest, OnLayoutDumpRequest)
    IPC_MESSAGE_HANDLER(ShellViewMsg_ReplicateLayoutTestRuntimeFlagsChanges,
                        OnReplicateLayoutTestRuntimeFlagsChanges)
    IPC_MESSAGE_HANDLER(ShellViewMsg_ReplicateTestConfiguration,
                        OnReplicateTestConfiguration)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetTestConfiguration,
                        OnSetTestConfiguration)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetupSecondaryRenderer,
                        OnSetupSecondaryRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestRenderFrameObserver::OnLayoutDumpRequest() {
  std::string dump =
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->TestRunner()
          ->DumpLayout(render_frame()->GetWebFrame());
  Send(new ShellViewHostMsg_LayoutDumpResponse(routing_id(), dump));
}

void LayoutTestRenderFrameObserver::OnReplicateLayoutTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_layout_test_runtime_flags) {
  LayoutTestRenderThreadObserver::GetInstance()
      ->test_interfaces()
      ->TestRunner()
      ->ReplicateLayoutTestRuntimeFlagsChanges(
          changed_layout_test_runtime_flags);
}

void LayoutTestRenderFrameObserver::OnReplicateTestConfiguration(
    const ShellTestConfiguration& test_config,
    const base::DictionaryValue&
        accumulated_layout_test_runtime_flags_changes) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnReplicateTestConfiguration(test_config);

  OnReplicateLayoutTestRuntimeFlagsChanges(
      accumulated_layout_test_runtime_flags_changes);
}

void LayoutTestRenderFrameObserver::OnSetTestConfiguration(
    const ShellTestConfiguration& test_config) {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetTestConfiguration(test_config);
}

void LayoutTestRenderFrameObserver::OnSetupSecondaryRenderer() {
  BlinkTestRunner::Get(render_frame()->GetRenderView())
      ->OnSetupSecondaryRenderer();
}

}  // namespace content
