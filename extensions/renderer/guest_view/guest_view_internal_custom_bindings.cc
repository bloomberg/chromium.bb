// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/guest_view/guest_view_container.h"
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
  // Allow for an optional callback parameter.
  CHECK(args.Length() >= 3 && args.Length() <= 4);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Guest Instance ID.
  CHECK(args[1]->IsInt32());
  // Attach Parameters.
  CHECK(args[2]->IsObject());
  // Optional Callback Function.
  CHECK(args.Length() < 4 || args[3]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a GuestViewContainer within
  // a RenderView.
  GuestViewContainer* guest_view_container =
      GuestViewContainer::FromID(context()->GetRenderView()->GetRoutingID(),
                                 element_instance_id);

  // TODO(fsamuel): Should we be reporting an error if the element instance ID
  // is invalid?
  if (!guest_view_container)
    return;

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

  guest_view_container->AttachGuest(
      element_instance_id,
      guest_instance_id,
      params.Pass(),
      args.Length() == 4 ? args[3].As<v8::Function>() :
          v8::Handle<v8::Function>(),
      args.GetIsolate());

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

}  // namespace extensions
