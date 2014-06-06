// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/page_capture_custom_bindings.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebBlob.h"
#include "v8/include/v8.h"

namespace extensions {

PageCaptureCustomBindings::PageCaptureCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("CreateBlob",
      base::Bind(&PageCaptureCustomBindings::CreateBlob,
                 base::Unretained(this)));
  RouteFunction("SendResponseAck",
      base::Bind(&PageCaptureCustomBindings::SendResponseAck,
                 base::Unretained(this)));
}

void PageCaptureCustomBindings::CreateBlob(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());
  blink::WebString path(base::UTF8ToUTF16(*v8::String::Utf8Value(args[0])));
  blink::WebBlob blob =
      blink::WebBlob::createFromFile(path, args[1]->Int32Value());
  args.GetReturnValue().Set(blob.toV8Value(args.Holder(), args.GetIsolate()));
}

void PageCaptureCustomBindings::SendResponseAck(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsInt32());

  content::RenderView* render_view = context()->GetRenderView();
  if (render_view) {
    render_view->Send(new ExtensionHostMsg_ResponseAck(
        render_view->GetRoutingID(), args[0]->Int32Value()));
  }
}

}  // namespace extensions
