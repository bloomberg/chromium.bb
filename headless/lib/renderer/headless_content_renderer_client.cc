// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "headless/lib/tab_socket.mojom.h"
#include "printing/features/features.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebIsolatedWorldIds.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "components/printing/renderer/print_web_view_helper.h"
#include "headless/lib/renderer/headless_print_web_view_helper_delegate.h"
#endif

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() {}

HeadlessContentRendererClient::~HeadlessContentRendererClient() {}

class HeadlessTabSocketBindings
    : public gin::Wrappable<HeadlessTabSocketBindings>,
      public content::RenderFrameObserver,
      public blink::WebScriptExecutionCallback {
 public:
  explicit HeadlessTabSocketBindings(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame), weak_ptr_factory_(this) {}
  ~HeadlessTabSocketBindings() override {}

  // content::RenderFrameObserver implementation:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override {
    if (world_id == content::ISOLATED_WORLD_ID_GLOBAL) {
      // For the main world only inject TabSocket if
      // BINDINGS_POLICY_HEADLESS_MAIN_WORLD is set.
      if (!(render_frame()->GetEnabledBindings() &
            content::BindingsPolicy::BINDINGS_POLICY_HEADLESS_MAIN_WORLD)) {
        return;
      }
    } else {
      // For the isolated worlds only inject TabSocket if
      // BINDINGS_POLICY_HEADLESS_ISOLATED_WORLD is set and the world id falls
      // within the range reserved for DevTools created isolated worlds.
      if (!(render_frame()->GetEnabledBindings() &
            content::BindingsPolicy::BINDINGS_POLICY_HEADLESS_ISOLATED_WORLD)) {
        return;
      }
      if (world_id < blink::kDevToolsFirstIsolatedWorldId ||
          world_id > blink::kDevToolsLastIsolatedWorldId) {
        return;
      }
    }

    InitializeTabSocketBindings(context);
  }

  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override {
    if (context_ == context) {
      on_message_callback_.Reset();
      context_.Reset();
    }
  }

  void OnDestruct() override {}

  // gin::WrappableBase implementation:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<HeadlessTabSocketBindings>::GetObjectTemplateBuilder(
               isolate)
        .SetMethod("send", &HeadlessTabSocketBindings::SendImpl)
        .SetProperty("onmessage", &HeadlessTabSocketBindings::GetOnMessage,
                     &HeadlessTabSocketBindings::SetOnMessage);
  }

  static gin::WrapperInfo kWrapperInfo;

 private:
  void SendImpl(const std::string& message) {
    EnsureTabSocketPtr()->SendMessageToEmbedder(message);
  }

  v8::Local<v8::Value> GetOnMessage() { return GetOnMessageCallback(); }

  void SetOnMessage(v8::Local<v8::Function> callback) {
    on_message_callback_.Reset(blink::MainThreadIsolate(), callback);

    EnsureTabSocketPtr()->AwaitNextMessageFromEmbedder(
        base::Bind(&HeadlessTabSocketBindings::OnNextMessageFromEmbedder,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnNextMessageFromEmbedder(const std::string& message) {
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = GetContext();
    v8::Local<v8::Value> argv[] = {
        gin::Converter<std::string>::ToV8(isolate, message),
    };

    render_frame()->GetWebFrame()->RequestExecuteV8Function(
        context, GetOnMessageCallback(), context->Global(), arraysize(argv),
        argv, this);

    EnsureTabSocketPtr()->AwaitNextMessageFromEmbedder(
        base::Bind(&HeadlessTabSocketBindings::OnNextMessageFromEmbedder,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void InitializeTabSocketBindings(v8::Local<v8::Context> context) {
    // Add TabSocket bindings to the DevTools created isolated world.
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    if (context.IsEmpty())
      return;

    v8::Context::Scope context_scope(context);
    gin::Handle<HeadlessTabSocketBindings> bindings =
        gin::CreateHandle(isolate, this);
    if (bindings.IsEmpty())
      return;

    v8::Local<v8::Object> global = context->Global();
    global->Set(gin::StringToV8(isolate, "TabSocket"), bindings.ToV8());
    context_.Reset(blink::MainThreadIsolate(), context);
  }

  headless::TabSocketPtr& EnsureTabSocketPtr() {
    if (!tab_socket_ptr_.is_bound()) {
      render_frame()->GetRemoteInterfaces()->GetInterface(
          mojo::MakeRequest(&tab_socket_ptr_));
    }
    return tab_socket_ptr_;
  }

  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(blink::MainThreadIsolate(), context_);
  }

  v8::Local<v8::Function> GetOnMessageCallback() {
    return v8::Local<v8::Function>::New(blink::MainThreadIsolate(),
                                        on_message_callback_);
  }

  headless::TabSocketPtr tab_socket_ptr_;
  v8::UniquePersistent<v8::Context> context_;
  v8::UniquePersistent<v8::Function> on_message_callback_;
  base::WeakPtrFactory<HeadlessTabSocketBindings> weak_ptr_factory_;
};

gin::WrapperInfo HeadlessTabSocketBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  new printing::PrintWebViewHelper(
      render_frame, base::MakeUnique<HeadlessPrintWebViewHelperDelegate>());
#endif
  new HeadlessTabSocketBindings(render_frame);
}

}  // namespace headless
