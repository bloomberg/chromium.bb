// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_connector_js_wrapper.h"

#include <memory>
#include <utility>

#include "mojo/edk/js/handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

gin::WrapperInfo BlinkConnectorJsWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};
const char BlinkConnectorJsWrapper::kModuleName[] =
    "content/public/renderer/connector";

BlinkConnectorJsWrapper::~BlinkConnectorJsWrapper() {}

// static
gin::Handle<BlinkConnectorJsWrapper> BlinkConnectorJsWrapper::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    service_manager::Connector* connector) {
  return gin::CreateHandle(
      isolate,
      new BlinkConnectorJsWrapper(isolate, context, connector->GetWeakPtr()));
}

gin::ObjectTemplateBuilder BlinkConnectorJsWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<BlinkConnectorJsWrapper>::GetObjectTemplateBuilder(isolate)
      .SetMethod("bindInterface", &BlinkConnectorJsWrapper::BindInterface)
      .SetMethod("addInterfaceOverrideForTesting",
                 &BlinkConnectorJsWrapper::AddOverrideForTesting)
      .SetMethod("clearInterfaceOverridesForTesting",
                 &BlinkConnectorJsWrapper::ClearOverridesForTesting);
}

mojo::Handle BlinkConnectorJsWrapper::BindInterface(
    const std::string& service_name,
    const std::string& interface_name) {
  mojo::MessagePipe pipe;
  if (connector_) {
    connector_->BindInterface(
        service_manager::Identity(service_name,
                                  service_manager::mojom::kInheritUserID),
        interface_name, std::move(pipe.handle0));
  }
  return pipe.handle1.release();
}

void BlinkConnectorJsWrapper::AddOverrideForTesting(
    const std::string& service_name,
    const std::string& interface_name,
    v8::Local<v8::Function> service_factory) {
  ScopedJsFactory factory(v8::Isolate::GetCurrent(), service_factory);
  service_manager::Connector::TestApi test_api(connector_.get());
  test_api.OverrideBinderForTesting(
      service_name, interface_name,
      base::Bind(&BlinkConnectorJsWrapper::CallJsFactory,
                 weak_factory_.GetWeakPtr(), factory));
}

void BlinkConnectorJsWrapper::ClearOverridesForTesting() {
  service_manager::Connector::TestApi test_api(connector_.get());
  test_api.ClearBinderOverrides();
}

BlinkConnectorJsWrapper::BlinkConnectorJsWrapper(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    base::WeakPtr<service_manager::Connector> connector)
    : isolate_(isolate),
      context_(isolate, context),
      connector_(connector),
      weak_factory_(this) {
  context_.SetWeak(this, &BlinkConnectorJsWrapper::ClearContext,
                   v8::WeakCallbackType::kParameter);
}

void BlinkConnectorJsWrapper::CallJsFactory(
    const ScopedJsFactory& factory,
    mojo::ScopedMessagePipeHandle pipe) {
  if (context_.IsEmpty())
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Handle<v8::Context> context = context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[] = {
      gin::ConvertToV8(isolate_, mojo::Handle(pipe.release().value()))};
  blink::WebLocalFrame::FrameForContext(context)
      ->CallFunctionEvenIfScriptDisabled(factory.Get(isolate_),
                                         v8::Undefined(isolate_), 1, argv);
}

// static
void BlinkConnectorJsWrapper::ClearContext(
    const v8::WeakCallbackInfo<BlinkConnectorJsWrapper>& data) {
  BlinkConnectorJsWrapper* registry = data.GetParameter();
  registry->context_.Reset();
}

}  // namespace content
