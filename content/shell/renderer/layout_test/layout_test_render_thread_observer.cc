// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_render_thread_observer.h"

#include "base/command_line.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
#include "content/common/input/input_event_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/layout_test/blink_test_runner.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "v8/include/v8.h"

using blink::WebFrame;
using blink::WebRuntimeFeatures;

namespace content {

namespace {
LayoutTestRenderThreadObserver* g_instance = NULL;
}

// static
LayoutTestRenderThreadObserver*
LayoutTestRenderThreadObserver::GetInstance() {
  return g_instance;
}

LayoutTestRenderThreadObserver::LayoutTestRenderThreadObserver()
    : test_delegate_(nullptr) {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);
  EnableRendererLayoutTestMode();

  // We always expose GC to layout tests.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    WebRuntimeFeatures::enableTestOnlyFeatures(true);
  }

  test_interfaces_.reset(new test_runner::WebTestInterfaces);
  test_interfaces_->ResetAll();
  test_interfaces_->SetSendWheelGestures(UseGestureBasedWheelScrolling());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    blink::setFontAntialiasingEnabledForTest(true);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlwaysUseComplexText)) {
    blink::setAlwaysUseComplexTextForTest(true);
  }
}

LayoutTestRenderThreadObserver::~LayoutTestRenderThreadObserver() {
  CHECK(g_instance == this);
  g_instance = NULL;
}

void LayoutTestRenderThreadObserver::SetTestDelegate(
    test_runner::WebTestDelegate* delegate) {
  test_interfaces_->SetDelegate(delegate);
  test_delegate_ = delegate;
}

void LayoutTestRenderThreadObserver::OnRenderProcessShutdown() {
  test_interfaces_.reset();
}

bool LayoutTestRenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestRenderThreadObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetWebKitSourceDir, OnSetWebKitSourceDir)
    IPC_MESSAGE_HANDLER(LayoutTestMsg_ReplicateLayoutTestRuntimeFlagsChanges,
                        OnReplicateLayoutTestRuntimeFlagsChanges)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestRenderThreadObserver::OnSetWebKitSourceDir(
    const base::FilePath& webkit_source_dir) {
  webkit_source_dir_ = webkit_source_dir;
}

void LayoutTestRenderThreadObserver::OnReplicateLayoutTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_layout_test_runtime_flags) {
  test_interfaces()->TestRunner()->ReplicateLayoutTestRuntimeFlagsChanges(
      changed_layout_test_runtime_flags);
}

}  // namespace content
