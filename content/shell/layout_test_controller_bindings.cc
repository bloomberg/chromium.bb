// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/layout_test_controller_bindings.h"

#include "base/string_piece.h"
#include "content/public/renderer/render_view.h"
#include "content/shell/shell_messages.h"
#include "grit/shell_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace content {

namespace {

base::StringPiece GetStringResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
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

v8::Handle<v8::Value> NotifyDone(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_NotifyDone(view->GetRoutingID()));
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

v8::Handle<v8::Value> SetWaitUntilDone(const v8::Arguments& args) {
  RenderView* view = GetCurrentRenderView();
  if (!view)
    return v8::Undefined();

  view->Send(new ShellViewHostMsg_WaitUntilDone(view->GetRoutingID()));
  return v8::Undefined();
}

}  // namespace

LayoutTestControllerBindings::LayoutTestControllerBindings()
    : v8::Extension("layout_test_controller.js",
                    GetStringResource(
                        IDR_CONTENT_SHELL_LAYOUT_TEST_CONTROLLER_JS).data(),
                    0,     // num dependencies.
                    NULL,  // dependencies array.
                    GetStringResource(
                        IDR_CONTENT_SHELL_LAYOUT_TEST_CONTROLLER_JS).size()) {
}

LayoutTestControllerBindings::~LayoutTestControllerBindings() {
}

v8::Handle<v8::FunctionTemplate>
LayoutTestControllerBindings::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("NotifyDone")))
    return v8::FunctionTemplate::New(NotifyDone);
  if (name->Equals(v8::String::New("SetDumpAsText")))
    return v8::FunctionTemplate::New(SetDumpAsText);
  if (name->Equals(v8::String::New("SetDumpChildFramesAsText")))
    return v8::FunctionTemplate::New(SetDumpChildFramesAsText);
  if (name->Equals(v8::String::New("SetWaitUntilDone")))
    return v8::FunctionTemplate::New(SetWaitUntilDone);

  NOTREACHED();
  return v8::FunctionTemplate::New();
}

}  // namespace content
