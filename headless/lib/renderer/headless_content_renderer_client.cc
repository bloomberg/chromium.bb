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
#include "headless/lib/headless_render_frame_controller.mojom.h"
#include "headless/lib/tab_socket.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "printing/features/features.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
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

class HeadlessTabSocketBindings
    : public gin::Wrappable<HeadlessTabSocketBindings>,
      public blink::WebScriptExecutionCallback {
 public:
  explicit HeadlessTabSocketBindings(content::RenderFrame* render_frame)
      : render_frame_(render_frame), weak_ptr_factory_(this) {}
  ~HeadlessTabSocketBindings() override {}

  void SetBindingsPolicy(MojoBindingsPolicy bindings_policy) {
    bindings_policy_ = bindings_policy;

    if (world_id_)
      return;

    // Update bindings for pre-existing script contexts.
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    if (bindings_policy == MojoBindingsPolicy::MAIN_WORLD) {
      InitializeTabSocketBindings(
          render_frame_->GetWebFrame()->MainWorldScriptContext(),
          content::ISOLATED_WORLD_ID_GLOBAL);
    } else if (bindings_policy == MojoBindingsPolicy::ISOLATED_WORLD) {
      // TODO(eseckler): We currently only support a single isolated world
      // context. InitializeTabSocketBindings will log a warning and ignore any
      // but the first isolated world if called multiple times here.
      for (const auto& entry : context_map_) {
        if (entry.first != content::ISOLATED_WORLD_ID_GLOBAL)
          InitializeTabSocketBindings(entry.second.Get(isolate), entry.first);
      }
    }
  }

  void DidCreateScriptContext(v8::Local<v8::Context> context, int world_id) {
    bool is_devtools_world = world_id >= blink::kDevToolsFirstIsolatedWorldId &&
                             world_id <= blink::kDevToolsLastIsolatedWorldId;
    if (world_id == content::ISOLATED_WORLD_ID_GLOBAL) {
      context_map_[world_id].Reset(blink::MainThreadIsolate(), context);

      // For the main world, only inject TabSocket if MAIN_WORLD policy is set.
      if (bindings_policy_ != MojoBindingsPolicy::MAIN_WORLD)
        return;
    } else if (is_devtools_world) {
      context_map_[world_id].Reset(blink::MainThreadIsolate(), context);

      // For devtools-created isolated worlds, only inject TabSocket if
      // ISOLATED_WORLD policy is set.
      if (bindings_policy_ != MojoBindingsPolicy::ISOLATED_WORLD)
        return;
    } else {
      // Other worlds don't receive TabSocket bindings.
      return;
    }

    InitializeTabSocketBindings(context, world_id);
  }

  void WillReleaseScriptContext(v8::Local<v8::Context> context, int world_id) {
    context_map_.erase(world_id);
    if (world_id_ && *world_id_ == world_id) {
      on_message_callback_.Reset();
      world_id_.reset();
    }
  }

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

    render_frame_->GetWebFrame()->RequestExecuteV8Function(
        context, GetOnMessageCallback(), context->Global(), arraysize(argv),
        argv, this);

    EnsureTabSocketPtr()->AwaitNextMessageFromEmbedder(
        base::Bind(&HeadlessTabSocketBindings::OnNextMessageFromEmbedder,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void InitializeTabSocketBindings(v8::Local<v8::Context> context,
                                   int world_id) {
    if (world_id_) {
      // TODO(eseckler): Support multiple isolated worlds inside the same frame.
      LOG(WARNING) << "TabSocket not created for world id " << world_id
                   << ". TabSocket only supports a single active world.";
      return;
    }

    DCHECK(context_map_.find(world_id) != context_map_.end());

    // Add TabSocket bindings to the context.
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
    world_id_ = world_id;
  }

  headless::TabSocketPtr& EnsureTabSocketPtr() {
    if (!tab_socket_ptr_.is_bound()) {
      render_frame_->GetRemoteInterfaces()->GetInterface(
          mojo::MakeRequest(&tab_socket_ptr_));
    }
    return tab_socket_ptr_;
  }

  v8::Local<v8::Context> GetContext() {
    if (!world_id_)
      return v8::Local<v8::Context>();
    DCHECK(context_map_.find(*world_id_) != context_map_.end());
    return context_map_[*world_id_].Get(blink::MainThreadIsolate());
  }

  v8::Local<v8::Function> GetOnMessageCallback() {
    return v8::Local<v8::Function>::New(blink::MainThreadIsolate(),
                                        on_message_callback_);
  }

  content::RenderFrame* render_frame_;
  MojoBindingsPolicy bindings_policy_ = MojoBindingsPolicy::NONE;
  headless::TabSocketPtr tab_socket_ptr_;
  base::Optional<int> world_id_;
  base::flat_map<int, v8::UniquePersistent<v8::Context>> context_map_;
  v8::UniquePersistent<v8::Function> on_message_callback_;
  base::WeakPtrFactory<HeadlessTabSocketBindings> weak_ptr_factory_;
};

gin::WrapperInfo HeadlessTabSocketBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

class HeadlessRenderFrameControllerImpl : public HeadlessRenderFrameController,
                                          public content::RenderFrameObserver {
 public:
  HeadlessRenderFrameControllerImpl(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame),
        tab_socket_bindings_(render_frame) {
    render_frame->GetInterfaceRegistry()->AddInterface(base::Bind(
        &HeadlessRenderFrameControllerImpl::OnRenderFrameControllerRequest,
        base::Unretained(this)));
  }

  void OnRenderFrameControllerRequest(
      const service_manager::BindSourceInfo& source_info,
      headless::HeadlessRenderFrameControllerRequest request) {
    headless_render_frame_controller_bindings_.AddBinding(this,
                                                          std::move(request));
  }

  // HeadlessRenderFrameController implementation:
  void AllowTabSocketBindings(
      MojoBindingsPolicy policy,
      AllowTabSocketBindingsCallback callback) override {
    tab_socket_bindings_.SetBindingsPolicy(policy);
    std::move(callback).Run();
  }

  // content::RenderFrameObserverW implementation:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override {
    tab_socket_bindings_.DidCreateScriptContext(context, world_id);
  }

  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override {
    tab_socket_bindings_.WillReleaseScriptContext(context, world_id);
  }

  void OnDestruct() override { delete this; }

 private:
  mojo::BindingSet<headless::HeadlessRenderFrameController>
      headless_render_frame_controller_bindings_;
  HeadlessTabSocketBindings tab_socket_bindings_;
};

HeadlessContentRendererClient::HeadlessContentRendererClient() {}

HeadlessContentRendererClient::~HeadlessContentRendererClient() {}

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  new printing::PrintWebViewHelper(
      render_frame, base::MakeUnique<HeadlessPrintWebViewHelperDelegate>());
#endif
  new HeadlessRenderFrameControllerImpl(render_frame);
}

}  // namespace headless
