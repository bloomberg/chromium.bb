// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner_bindings.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/shell_render_process_observer.h"
#include "content/shell/webkit_test_runner.h"
#include "grit/shell_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace content {

namespace {

base::StringPiece GetStringResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id);
}

v8::Handle<v8::Value> NotifyDone(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->NotifyDone();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetDumpAsText(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpAsText();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetDumpChildFramesAsText(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpChildFramesAsText();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetWaitUntilDone(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->WaitUntilDone();
  return v8::Undefined();
}

v8::Handle<v8::Value> OverridePreference(const v8::Arguments& args) {
  if (args.Length() != 2 || !args[0]->IsString())
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->OverridePreference(*v8::String::AsciiValue(args[0]), args[1]);
  return v8::Undefined();
}

v8::Handle<v8::Value> NotImplemented(const v8::Arguments& args) {
  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString())
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->NotImplemented(*v8::String::AsciiValue(args[0]),
                         *v8::String::AsciiValue(args[1]));
  return v8::Undefined();
}

}  // namespace

WebKitTestRunnerBindings::WebKitTestRunnerBindings()
    : v8::Extension("webkit_test_runner.js",
                    GetStringResource(
                        IDR_CONTENT_SHELL_WEBKIT_TEST_RUNNER_JS).data(),
                    0,     // num dependencies.
                    NULL,  // dependencies array.
                    GetStringResource(
                        IDR_CONTENT_SHELL_WEBKIT_TEST_RUNNER_JS).size()) {
}

WebKitTestRunnerBindings::~WebKitTestRunnerBindings() {
}

v8::Handle<v8::FunctionTemplate>
WebKitTestRunnerBindings::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("NotifyDone")))
    return v8::FunctionTemplate::New(NotifyDone);
  if (name->Equals(v8::String::New("SetDumpAsText")))
    return v8::FunctionTemplate::New(SetDumpAsText);
  if (name->Equals(v8::String::New("SetDumpChildFramesAsText")))
    return v8::FunctionTemplate::New(SetDumpChildFramesAsText);
  if (name->Equals(v8::String::New("SetWaitUntilDone")))
    return v8::FunctionTemplate::New(SetWaitUntilDone);
  if (name->Equals(v8::String::New("OverridePreference")))
    return v8::FunctionTemplate::New(OverridePreference);
  if (name->Equals(v8::String::New("NotImplemented")))
    return v8::FunctionTemplate::New(NotImplemented);

  NOTREACHED();
  return v8::FunctionTemplate::New();
}

}  // namespace content
