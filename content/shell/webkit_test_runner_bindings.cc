// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner_bindings.h"

#include "base/string_piece.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/shell_messages.h"
#include "content/shell/webkit_test_runner.h"
#include "grit/shell_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerInfo.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebFrame;
using WebKit::WebView;
using WebKit::WebWorkerInfo;

namespace content {

namespace {

base::StringPiece GetStringResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id);
}

RenderView* GetCurrentRenderView() {
  WebFrame* frame = WebFrame::frameForCurrentContext();
  DCHECK(frame);
  if (!frame)
    return NULL;

  WebView* view = frame->view();
  if (!view)
    return NULL;  // can happen during closing.

  RenderView* render_view = RenderView::FromWebView(view);
  DCHECK(render_view);
  return render_view;
}

v8::Handle<v8::Value> Display(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();
  WebKitTestRunner* runner = WebKitTestRunner::Get(view);
  if (!runner)
    return v8::Undefined();
  runner->Display();
  return v8::Undefined();
}

v8::Handle<v8::Value> NotifyDone(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_NotifyDone(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetCanOpenWindows(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_CanOpenWindows(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetDumpAsText(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_DumpAsText(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetDumpChildFramesAsText(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_DumpChildFramesAsText(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetPrinting(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_SetPrinting(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetShouldStayOnPageAfterHandlingBeforeUnload(
    const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  if (args.Length() != 1 || !args[0]->IsBoolean())
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_SetShouldStayOnPageAfterHandlingBeforeUnload(
      view->GetRoutingID(), args[0]->BooleanValue()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetWaitUntilDone(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_WaitUntilDone(view->GetRoutingID()));
  return v8::Undefined();
}

v8::Handle<v8::Value> SetXSSAuditorEnabled(
    const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  if (args.Length() != 1 || !args[0]->IsBoolean())
    return v8::Undefined();

  WebKitTestRunner* runner = WebKitTestRunner::Get(view);
  if (!runner)
    return v8::Undefined();

  runner->SetXSSAuditorEnabled(args[0]->BooleanValue());
  return v8::Undefined();
}

v8::Handle<v8::Value> GetWorkerThreadCount(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  return v8::Integer::NewFromUnsigned(WebWorkerInfo::dedicatedWorkerCount());
}

v8::Handle<v8::Value> NotImplemented(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString())
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_NotImplemented(
      view->GetRoutingID(),
      *v8::String::AsciiValue(args[0]),
      *v8::String::AsciiValue(args[1])));
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
  if (name->Equals(v8::String::New("NotImplemented")))
    return v8::FunctionTemplate::New(NotImplemented);

  NOTREACHED();
  return v8::FunctionTemplate::New();
}

}  // namespace content
