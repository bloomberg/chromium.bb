// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

GuestViewInternalCustomBindings::GuestViewInternalCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("AttachGuest",
                base::Bind(&GuestViewInternalCustomBindings::AttachGuest,
                           base::Unretained(this)));
}

void GuestViewInternalCustomBindings::AttachGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 3 && args[0]->IsInt32() && args[1]->IsInt32() &&
      args[2]->IsObject());

  content::RenderFrame* render_frame = context()->GetRenderFrame();
  if (!render_frame)
    return;

  int element_instance_id = args[0]->Int32Value();
  int guest_instance_id = args[1]->Int32Value();

  scoped_ptr<base::DictionaryValue> params;
  {
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    scoped_ptr<base::Value> params_as_value(
        converter->FromV8Value(args[2], context()->v8_context()));
    CHECK(params_as_value->IsType(base::Value::TYPE_DICTIONARY));
    params.reset(
        static_cast<base::DictionaryValue*>(params_as_value.release()));
  }

  // Step 1, send the attach params to chrome/.
  render_frame->Send(new ExtensionHostMsg_AttachGuest(
      render_frame->GetRenderView()->GetRoutingID(),
      element_instance_id,
      guest_instance_id,
      *params));

  // Step 2, attach plugin through content/.
  render_frame->AttachGuest(element_instance_id);

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

}  // namespace extensions
