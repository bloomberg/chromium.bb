// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

GuestViewInternalCustomBindings::GuestViewInternalCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("AttachGuest",
                base::Bind(&GuestViewInternalCustomBindings::AttachGuest,
                           base::Unretained(this)));
  RouteFunction("DetachGuest",
                base::Bind(&GuestViewInternalCustomBindings::DetachGuest,
                           base::Unretained(this)));
  RouteFunction("GetContentWindow",
                base::Bind(&GuestViewInternalCustomBindings::GetContentWindow,
                           base::Unretained(this)));
  RouteFunction(
      "RegisterDestructionCallback",
      base::Bind(&GuestViewInternalCustomBindings::RegisterDestructionCallback,
                 base::Unretained(this)));
  RouteFunction(
      "RegisterElementResizeCallback",
      base::Bind(
          &GuestViewInternalCustomBindings::RegisterElementResizeCallback,
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
  // An element instance ID uniquely identifies a ExtensionsGuestViewContainer.
  ExtensionsGuestViewContainer* guest_view_container =
      ExtensionsGuestViewContainer::FromID(element_instance_id);

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

  // Add flag to |params| to indicate that the element size is specified in
  // logical units.
  params->SetBoolean(guestview::kElementSizeIsLogical, true);

  linked_ptr<ExtensionsGuestViewContainer::Request> request(
      new ExtensionsGuestViewContainer::AttachRequest(
          guest_view_container,
          guest_instance_id,
          params.Pass(),
          args.Length() == 4 ? args[3].As<v8::Function>() :
              v8::Handle<v8::Function>(),
          args.GetIsolate()));
  guest_view_container->IssueRequest(request);

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::DetachGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Allow for an optional callback parameter.
  CHECK(args.Length() >= 1 && args.Length() <= 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Optional Callback Function.
  CHECK(args.Length() < 2 || args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a ExtensionsGuestViewContainer.
  ExtensionsGuestViewContainer* guest_view_container =
      ExtensionsGuestViewContainer::FromID(element_instance_id);

  // TODO(fsamuel): Should we be reporting an error if the element instance ID
  // is invalid?
  if (!guest_view_container)
    return;

  linked_ptr<ExtensionsGuestViewContainer::Request> request(
      new ExtensionsGuestViewContainer::DetachRequest(
          guest_view_container,
          args.Length() == 2 ? args[1].As<v8::Function>() :
              v8::Handle<v8::Function>(),
          args.GetIsolate()));
  guest_view_container->IssueRequest(request);

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::GetContentWindow(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Default to returning null.
  args.GetReturnValue().SetNull();

  if (args.Length() != 1)
    return;

  // The routing ID for the RenderView.
  if (!args[0]->IsInt32())
    return;

  int view_id = args[0]->Int32Value();
  if (view_id == MSG_ROUTING_NONE)
    return;

  content::RenderView* view = content::RenderView::FromRoutingID(view_id);
  if (!view)
    return;

  blink::WebFrame* frame = view->GetWebView()->mainFrame();
  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();
  args.GetReturnValue().Set(window);
}

void GuestViewInternalCustomBindings::RegisterDestructionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are two parameters.
  CHECK(args.Length() == 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Callback function.
  CHECK(args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a ExtensionsGuestViewContainer
  // within a RenderView.
  ExtensionsGuestViewContainer* guest_view_container =
      ExtensionsGuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  guest_view_container->RegisterDestructionCallback(args[1].As<v8::Function>(),
                                                    args.GetIsolate());

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::RegisterElementResizeCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are two parameters.
  CHECK(args.Length() == 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Callback function.
  CHECK(args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a ExtensionsGuestViewContainer
  // within a RenderView.
  ExtensionsGuestViewContainer* guest_view_container =
      ExtensionsGuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  guest_view_container->RegisterElementResizeCallback(
      args[1].As<v8::Function>(), args.GetIsolate());

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

}  // namespace extensions
