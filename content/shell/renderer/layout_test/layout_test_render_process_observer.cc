// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_render_process_observer.h"

#include "base/command_line.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/webkit_test_runner.h"
#include "content/shell/renderer/test_runner/web_test_interfaces.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using blink::WebFrame;
using blink::WebRuntimeFeatures;

namespace content {

namespace {
LayoutTestRenderProcessObserver* g_instance = NULL;
}

// static
LayoutTestRenderProcessObserver*
LayoutTestRenderProcessObserver::GetInstance() {
  return g_instance;
}

LayoutTestRenderProcessObserver::LayoutTestRenderProcessObserver()
    : main_test_runner_(NULL),
      test_delegate_(NULL) {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);
  EnableRendererLayoutTestMode();
}

LayoutTestRenderProcessObserver::~LayoutTestRenderProcessObserver() {
  CHECK(g_instance == this);
  g_instance = NULL;
}

void LayoutTestRenderProcessObserver::SetTestDelegate(
    WebTestDelegate* delegate) {
  test_interfaces_->SetDelegate(delegate);
  test_delegate_ = delegate;
}

void LayoutTestRenderProcessObserver::SetMainWindow(RenderView* view) {
  WebKitTestRunner* test_runner = WebKitTestRunner::Get(view);
  test_interfaces_->SetWebView(view->GetWebView(), test_runner->proxy());
  main_test_runner_ = test_runner;
}

void LayoutTestRenderProcessObserver::WebKitInitialized() {
  // We always expose GC to layout tests.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    WebRuntimeFeatures::enableTestOnlyFeatures(true);
  }

  test_interfaces_.reset(new WebTestInterfaces);
  test_interfaces_->ResetAll();
}

void LayoutTestRenderProcessObserver::OnRenderProcessShutdown() {
  test_interfaces_.reset();
}

bool LayoutTestRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetWebKitSourceDir, OnSetWebKitSourceDir)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestRenderProcessObserver::OnSetWebKitSourceDir(
    const base::FilePath& webkit_source_dir) {
  webkit_source_dir_ = webkit_source_dir;
}

}  // namespace content
