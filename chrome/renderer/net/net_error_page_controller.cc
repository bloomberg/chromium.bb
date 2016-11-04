// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_page_controller.h"

#include "base/strings/string_piece.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

gin::WrapperInfo NetErrorPageController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

NetErrorPageController::Delegate::Delegate() {}
NetErrorPageController::Delegate::~Delegate() {}

// static
void NetErrorPageController::Install(content::RenderFrame* render_frame,
                                     base::WeakPtr<Delegate> delegate) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<NetErrorPageController> controller = gin::CreateHandle(
      isolate, new NetErrorPageController(delegate));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "errorPageController"),
              controller.ToV8());
}

bool NetErrorPageController::ShowSavedCopyButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::SHOW_SAVED_COPY_BUTTON);
}

bool NetErrorPageController::DownloadButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::DOWNLOAD_BUTTON);
}

bool NetErrorPageController::ReloadButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::RELOAD_BUTTON);
}

bool NetErrorPageController::DetailsButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::MORE_BUTTON);
}

bool NetErrorPageController::TrackEasterEgg() {
  return ButtonClick(error_page::NetErrorHelperCore::EASTER_EGG);
}

bool NetErrorPageController::DiagnoseErrorsButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::DIAGNOSE_ERROR);
}

bool NetErrorPageController::TrackCachedCopyButtonClick() {
  return ButtonClick(error_page::NetErrorHelperCore::SHOW_CACHED_COPY_BUTTON);
}

bool NetErrorPageController::TrackClick(const gin::Arguments& args) {
  if (args.PeekNext().IsEmpty() || !args.PeekNext()->IsInt32())
    return false;

  if (delegate_)
    delegate_->TrackClick(args.PeekNext()->Int32Value());
  return true;
}

bool NetErrorPageController::ButtonClick(
    error_page::NetErrorHelperCore::Button button) {
  if (delegate_)
    delegate_->ButtonPressed(button);

  return true;
}

NetErrorPageController::NetErrorPageController(base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {
}

NetErrorPageController::~NetErrorPageController() {}

gin::ObjectTemplateBuilder NetErrorPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NetErrorPageController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("showSavedCopyButtonClick",
                 &NetErrorPageController::ShowSavedCopyButtonClick)
      .SetMethod("downloadButtonClick",
                 &NetErrorPageController::DownloadButtonClick)
      .SetMethod("reloadButtonClick",
                 &NetErrorPageController::ReloadButtonClick)
      .SetMethod("detailsButtonClick",
                 &NetErrorPageController::DetailsButtonClick)
      .SetMethod("diagnoseErrorsButtonClick",
                 &NetErrorPageController::DiagnoseErrorsButtonClick)
      .SetMethod("trackClick", &NetErrorPageController::TrackClick)
      .SetMethod("trackEasterEgg", &NetErrorPageController::TrackEasterEgg)
      .SetMethod("trackCachedCopyButtonClick",
                 &NetErrorPageController::TrackCachedCopyButtonClick);
}
