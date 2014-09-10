// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_page_controller.h"

#include "base/strings/string_piece.h"
#include "chrome/renderer/net/net_error_helper.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

gin::WrapperInfo NetErrorPageController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void NetErrorPageController::Install(content::RenderFrame* render_frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context =
      render_frame->GetWebFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<NetErrorPageController> controller = gin::CreateHandle(
      isolate, new NetErrorPageController(render_frame));
  if (controller.IsEmpty())
    return;

  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "errorPageController"),
              controller.ToV8());
}

bool NetErrorPageController::LoadStaleButtonClick() {
  if (!render_frame())
    return false;

  NetErrorHelper* net_error_helper =
      content::RenderFrameObserverTracker<NetErrorHelper>::Get(render_frame());
  DCHECK(net_error_helper);
  net_error_helper->LoadStaleButtonPressed();

  return true;
}

bool NetErrorPageController::ReloadButtonClick() {
  if (!render_frame())
    return false;

  NetErrorHelper* net_error_helper =
      content::RenderFrameObserverTracker<NetErrorHelper>::Get(render_frame());
  DCHECK(net_error_helper);
  net_error_helper->ReloadButtonPressed();

  return true;
}

bool NetErrorPageController::DetailsButtonClick() {
  if (!render_frame())
    return false;

  NetErrorHelper* net_error_helper =
      content::RenderFrameObserverTracker<NetErrorHelper>::Get(render_frame());
  DCHECK(net_error_helper);
  net_error_helper->MoreButtonPressed();

  return true;
}

bool NetErrorPageController::TrackClick(const gin::Arguments& args) {
  if (!render_frame())
    return false;

  if (!args.PeekNext()->IsInt32())
    return false;

  NetErrorHelper* net_error_helper =
      content::RenderFrameObserverTracker<NetErrorHelper>::Get(render_frame());
  DCHECK(net_error_helper);
  net_error_helper->TrackClick(args.PeekNext()->Int32Value());
  return true;
}

NetErrorPageController::NetErrorPageController(
    content::RenderFrame* render_frame) : RenderFrameObserver(render_frame) {}

NetErrorPageController::~NetErrorPageController() {}

gin::ObjectTemplateBuilder NetErrorPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NetErrorPageController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("loadStaleButtonClick",
                 &NetErrorPageController::LoadStaleButtonClick)
      .SetMethod("reloadButtonClick",
                 &NetErrorPageController::ReloadButtonClick)
      .SetMethod("detailsButtonClick",
                 &NetErrorPageController::DetailsButtonClick)
      .SetMethod("trackClick",
                 &NetErrorPageController::TrackClick);
}

void NetErrorPageController::OnDestruct() {}
