// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_render_process_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/shell_messages.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_runner.h"
#include "content/shell/webkit_test_runner_bindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTestingSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/support/gc_extension.h"

using WebKit::WebFrame;
using WebKit::WebRuntimeFeatures;
using WebKit::WebTestingSupport;
using WebTestRunner::WebTestDelegate;

namespace content {

namespace {
ShellRenderProcessObserver* g_instance = NULL;
}

// static
ShellRenderProcessObserver* ShellRenderProcessObserver::GetInstance() {
  return g_instance;
}

ShellRenderProcessObserver::ShellRenderProcessObserver()
    : main_render_view_(NULL),
      main_test_runner_(NULL),
      test_delegate_(NULL) {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  WebRuntimeFeatures::enableInputTypeDateTime(true);
  WebRuntimeFeatures::enableInputTypeDateTimeLocal(true);
  WebRuntimeFeatures::enableInputTypeMonth(true);
  WebRuntimeFeatures::enableInputTypeTime(true);
  WebRuntimeFeatures::enableInputTypeWeek(true);
  DisableAppCacheLogging();
  EnableDevToolsFrontendTesting();
}

ShellRenderProcessObserver::~ShellRenderProcessObserver() {
  CHECK(g_instance == this);
  g_instance = NULL;
}

void ShellRenderProcessObserver::SetMainWindow(
    RenderView* view,
    WebKitTestRunner* test_runner,
    WebTestDelegate* delegate) {
  main_render_view_ = view;
  main_test_runner_ = test_runner;
  test_delegate_ = delegate;
}

void ShellRenderProcessObserver::BindTestRunnersToWindow(WebFrame* frame) {
  WebTestingSupport::injectInternalsObject(frame);
}

void ShellRenderProcessObserver::WebKitInitialized() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;

  // To implement a catch-all for not yet implemented controller properties.
  webkit_glue::SetJavaScriptFlags(" --harmony_proxies");
  RenderThread::Get()->RegisterExtension(new WebKitTestRunnerBindings());

  // We always expose GC to layout tests.
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
  RenderThread::Get()->RegisterExtension(extensions_v8::GCExtension::Get());
}

bool ShellRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ShellViewMsg_ResetAll, OnResetAll)
    IPC_MESSAGE_HANDLER(ShellViewMsg_SetWebKitSourceDir, OnSetWebKitSourceDir)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ShellRenderProcessObserver::OnResetAll() {
  if (main_render_view_) {
    main_test_runner_->Reset();
    WebTestingSupport::resetInternalsObject(
        main_render_view_->GetWebView()->mainFrame());
  }
}

void ShellRenderProcessObserver::OnSetWebKitSourceDir(
    const base::FilePath& webkit_source_dir) {
  webkit_source_dir_ = webkit_source_dir;
}

}  // namespace content
