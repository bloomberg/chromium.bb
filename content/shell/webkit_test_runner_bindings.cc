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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerInfo.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebWorkerInfo;

namespace content {

namespace {

bool g_global_flag = false;

base::StringPiece GetStringResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id);
}

v8::Handle<v8::Value> Display(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->Display();
  return v8::Undefined();
}

v8::Handle<v8::Value> NotifyDone(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->NotifyDone();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetCanOpenWindows(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->CanOpenWindows();
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

v8::Handle<v8::Value> SetPrinting(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->SetPrinting();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetShouldStayOnPageAfterHandlingBeforeUnload(
    const v8::Arguments& args) {
  if (args.Length() != 1 || !args[0]->IsBoolean())
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->SetShouldStayOnPageAfterHandlingBeforeUnload(args[0]->BooleanValue());
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

v8::Handle<v8::Value> SetXSSAuditorEnabled(
    const v8::Arguments& args) {
  if (args.Length() != 1 || !args[0]->IsBoolean())
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->SetXSSAuditorEnabled(args[0]->BooleanValue());
  return v8::Undefined();
}

v8::Handle<v8::Value> ShowWebInspector(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->ShowWebInspector();
  return v8::Undefined();
}

v8::Handle<v8::Value> CloseWebInspector(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->CloseWebInspector();
  return v8::Undefined();
}

v8::Handle<v8::Value> EvaluateInWebInspector(const v8::Arguments& args) {
  if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsString())
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->EvaluateInWebInspector(args[0]->Int32Value(),
                                 *v8::String::AsciiValue(args[1]));
  return v8::Undefined();
}

v8::Handle<v8::Value> ExecCommand(const v8::Arguments& args) {
  // This method takes one, two, or three parameters, however, we only care
  // about the first and third parameter which (if present) need to be strings.
  // We ignore the second parameter (user interface) since this command
  // emulates a manual action.
  if (args.Length() == 0 || args.Length() > 3)
    return v8::Undefined();

  if (!args[0]->IsString() || (args.Length() == 3 && !args[2]->IsString()))
    return v8::Undefined();

  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  std::string command(*v8::String::AsciiValue(args[0]));
  std::string value;
  if (args.Length() == 3)
    value = *v8::String::AsciiValue(args[2]);

  runner->ExecCommand(command, value);
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

v8::Handle<v8::Value> DumpEditingCallbacks(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpEditingCallbacks();
  return v8::Undefined();
}

v8::Handle<v8::Value> DumpFrameLoadCallbacks(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpFrameLoadCallbacks();
  return v8::Undefined();
}

v8::Handle<v8::Value> DumpUserGestureInFrameLoadCallbacks(
    const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpUserGestureInFrameLoadCallbacks();
  return v8::Undefined();
}

v8::Handle<v8::Value> SetStopProvisionalFrameLoads(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->StopProvisionalFrameLoads();
  return v8::Undefined();
}

v8::Handle<v8::Value> DumpTitleChanges(const v8::Arguments& args) {
  WebKitTestRunner* runner =
      ShellRenderProcessObserver::GetInstance()->main_test_runner();
  if (!runner)
    return v8::Undefined();

  runner->DumpTitleChanges();
  return v8::Undefined();
}

v8::Handle<v8::Value> GetGlobalFlag(const v8::Arguments& args) {
  return v8::Boolean::New(g_global_flag);
}

v8::Handle<v8::Value> SetGlobalFlag(const v8::Arguments& args) {
  if (args.Length() == 1 && args[0]->IsBoolean())
    g_global_flag = args[0]->BooleanValue();
  return v8::Undefined();
}

v8::Handle<v8::Value> GetWorkerThreadCount(const v8::Arguments& args) {
  return v8::Integer::NewFromUnsigned(WebWorkerInfo::dedicatedWorkerCount());
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

// static
void WebKitTestRunnerBindings::Reset() {
  g_global_flag = false;
}

v8::Handle<v8::FunctionTemplate>
WebKitTestRunnerBindings::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("Display")))
    return v8::FunctionTemplate::New(Display);
  if (name->Equals(v8::String::New("NotifyDone")))
    return v8::FunctionTemplate::New(NotifyDone);
  if (name->Equals(v8::String::New("SetCanOpenWindows")))
    return v8::FunctionTemplate::New(SetCanOpenWindows);
  if (name->Equals(v8::String::New("SetDumpAsText")))
    return v8::FunctionTemplate::New(SetDumpAsText);
  if (name->Equals(v8::String::New("SetDumpChildFramesAsText")))
    return v8::FunctionTemplate::New(SetDumpChildFramesAsText);
  if (name->Equals(v8::String::New("SetPrinting")))
    return v8::FunctionTemplate::New(SetPrinting);
  if (name->Equals(v8::String::New(
          "SetShouldStayOnPageAfterHandlingBeforeUnload"))) {
    return v8::FunctionTemplate::New(
        SetShouldStayOnPageAfterHandlingBeforeUnload);
  }
  if (name->Equals(v8::String::New("SetWaitUntilDone")))
    return v8::FunctionTemplate::New(SetWaitUntilDone);
  if (name->Equals(v8::String::New("SetXSSAuditorEnabled")))
    return v8::FunctionTemplate::New(SetXSSAuditorEnabled);
  if (name->Equals(v8::String::New("GetWorkerThreadCount")))
    return v8::FunctionTemplate::New(GetWorkerThreadCount);
  if (name->Equals(v8::String::New("ShowWebInspector")))
    return v8::FunctionTemplate::New(ShowWebInspector);
  if (name->Equals(v8::String::New("CloseWebInspector")))
    return v8::FunctionTemplate::New(CloseWebInspector);
  if (name->Equals(v8::String::New("EvaluateInWebInspector")))
    return v8::FunctionTemplate::New(EvaluateInWebInspector);
  if (name->Equals(v8::String::New("ExecCommand")))
    return v8::FunctionTemplate::New(ExecCommand);
  if (name->Equals(v8::String::New("OverridePreference")))
    return v8::FunctionTemplate::New(OverridePreference);
  if (name->Equals(v8::String::New("DumpEditingCallbacks")))
    return v8::FunctionTemplate::New(DumpEditingCallbacks);
  if (name->Equals(v8::String::New("DumpFrameLoadCallbacks")))
    return v8::FunctionTemplate::New(DumpFrameLoadCallbacks);
  if (name->Equals(v8::String::New("DumpUserGestureInFrameLoadCallbacks")))
    return v8::FunctionTemplate::New(DumpUserGestureInFrameLoadCallbacks);
  if (name->Equals(v8::String::New("SetStopProvisionalFrameLoads")))
    return v8::FunctionTemplate::New(SetStopProvisionalFrameLoads);
  if (name->Equals(v8::String::New("DumpTitleChanges")))
    return v8::FunctionTemplate::New(DumpTitleChanges);
  if (name->Equals(v8::String::New("GetGlobalFlag")))
    return v8::FunctionTemplate::New(GetGlobalFlag);
  if (name->Equals(v8::String::New("SetGlobalFlag")))
    return v8::FunctionTemplate::New(SetGlobalFlag);
  if (name->Equals(v8::String::New("NotImplemented")))
    return v8::FunctionTemplate::New(NotImplemented);

  NOTREACHED();
  return v8::FunctionTemplate::New();
}

}  // namespace content
