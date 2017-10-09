// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"

#include <map>
#include <set>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderOptions.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {

namespace {

const char kPostMessageName[] = "postMessage";

// The gin-backed scriptable object which is exposed by the BrowserPlugin for
// MimeHandlerViewContainer. This currently only implements "postMessage".
class ScriptableObject : public gin::Wrappable<ScriptableObject>,
                         public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static v8::Local<v8::Object> Create(
      v8::Isolate* isolate,
      base::WeakPtr<MimeHandlerViewContainer> container) {
    ScriptableObject* scriptable_object =
        new ScriptableObject(isolate, container);
    return gin::CreateHandle(isolate, scriptable_object)
        .ToV8()
        .As<v8::Object>();
  }

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(
      v8::Isolate* isolate,
      const std::string& identifier) override {
    if (identifier == kPostMessageName) {
      if (post_message_function_template_.IsEmpty()) {
        post_message_function_template_.Reset(
            isolate,
            gin::CreateFunctionTemplate(
                isolate, base::Bind(&MimeHandlerViewContainer::PostMessage,
                                    container_, isolate)));
      }
      v8::Local<v8::FunctionTemplate> function_template =
          v8::Local<v8::FunctionTemplate>::New(isolate,
                                               post_message_function_template_);
      v8::Local<v8::Function> function;
      if (function_template->GetFunction(isolate->GetCurrentContext())
              .ToLocal(&function))
        return function;
    }
    return v8::Local<v8::Value>();
  }

 private:
  ScriptableObject(v8::Isolate* isolate,
                   base::WeakPtr<MimeHandlerViewContainer> container)
    : gin::NamedPropertyInterceptor(isolate, this),
      container_(container) {}

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<ScriptableObject>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor();
  }

  base::WeakPtr<MimeHandlerViewContainer> container_;
  v8::Persistent<v8::FunctionTemplate> post_message_function_template_;
};

// static
gin::WrapperInfo ScriptableObject::kWrapperInfo = { gin::kEmbedderNativeGin };

// Maps from content::RenderFrame to the set of MimeHandlerViewContainers within
// it.
base::LazyInstance<
    std::map<content::RenderFrame*, std::set<MimeHandlerViewContainer*>>>::
    DestructorAtExit g_mime_handler_view_container_map =
        LAZY_INSTANCE_INITIALIZER;

}  // namespace

MimeHandlerViewContainer::MimeHandlerViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url)
    : GuestViewContainer(render_frame),
      mime_type_(mime_type),
      original_url_(original_url),
      guest_proxy_routing_id_(-1),
      guest_loaded_(false),
      weak_factory_(this) {
  DCHECK(!mime_type_.empty());
  is_embedded_ = !render_frame->GetWebFrame()->GetDocument().IsPluginDocument();
  g_mime_handler_view_container_map.Get()[render_frame].insert(this);
}

MimeHandlerViewContainer::~MimeHandlerViewContainer() {
  if (loader_) {
    DCHECK(is_embedded_);
    loader_->Cancel();
  }

  if (render_frame()) {
    g_mime_handler_view_container_map.Get()[render_frame()].erase(this);
    if (g_mime_handler_view_container_map.Get()[render_frame()].empty())
      g_mime_handler_view_container_map.Get().erase(render_frame());
  }
}

// static
std::vector<MimeHandlerViewContainer*>
MimeHandlerViewContainer::FromRenderFrame(content::RenderFrame* render_frame) {
  auto it = g_mime_handler_view_container_map.Get().find(render_frame);
  if (it == g_mime_handler_view_container_map.Get().end())
    return std::vector<MimeHandlerViewContainer*>();

  return std::vector<MimeHandlerViewContainer*>(it->second.begin(),
                                                it->second.end());
}

void MimeHandlerViewContainer::OnReady() {
  if (!render_frame() || !is_embedded_)
    return;

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  blink::WebAssociatedURLLoaderOptions options;
  DCHECK(!loader_);
  loader_.reset(frame->CreateAssociatedURLLoader(options));

  // The embedded plugin is allowed to be cross-origin and we should always
  // send credentials/cookies with the request. So, use the default mode
  // "no-cors" and credentials mode "include".
  blink::WebURLRequest request(original_url_);
  request.SetRequestContext(blink::WebURLRequest::kRequestContextObject);
  loader_->LoadAsynchronously(request, this);
}

bool MimeHandlerViewContainer::OnMessage(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeHandlerViewContainer, message)
  IPC_MESSAGE_HANDLER(ExtensionsGuestViewMsg_CreateMimeHandlerViewGuestACK,
                      OnCreateMimeHandlerViewGuestACK)
  IPC_MESSAGE_HANDLER(
      ExtensionsGuestViewMsg_MimeHandlerViewGuestOnLoadCompleted,
      OnMimeHandlerViewGuestOnLoadCompleted)
  IPC_MESSAGE_HANDLER(GuestViewMsg_GuestAttached, OnGuestAttached)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeHandlerViewContainer::PluginDidFinishLoading() {
  DCHECK(!is_embedded_);
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewContainer::OnRenderFrameDestroyed() {
  g_mime_handler_view_container_map.Get().erase(render_frame());
}

void MimeHandlerViewContainer::PluginDidReceiveData(const char* data,
                                                    int data_length) {
  view_id_ += std::string(data, data_length);
}


void MimeHandlerViewContainer::DidResizeElement(const gfx::Size& new_size) {
  element_size_ = new_size;

  CreateMimeHandlerViewGuestIfNecessary();

  // Don't try to resize a guest that hasn't been created yet. It is enough to
  // initialise |element_size_| here and then we'll send that to the browser
  // during guest creation.
  if (!guest_created_)
    return;

  render_frame()->Send(new ExtensionsGuestViewHostMsg_ResizeGuest(
      render_frame()->GetRoutingID(), element_instance_id(), new_size));
}

v8::Local<v8::Object> MimeHandlerViewContainer::V8ScriptableObject(
    v8::Isolate* isolate) {
  if (scriptable_object_.IsEmpty()) {
    v8::Local<v8::Object> object =
        ScriptableObject::Create(isolate, weak_factory_.GetWeakPtr());
    scriptable_object_.Reset(isolate, object);
  }
  return v8::Local<v8::Object>::New(isolate, scriptable_object_);
}

void MimeHandlerViewContainer::DidReceiveData(const char* data,
                                              int data_length) {
  view_id_ += std::string(data, data_length);
}

void MimeHandlerViewContainer::DidFinishLoading(double /* unused */) {
  DCHECK(is_embedded_);
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewContainer::PostMessage(v8::Isolate* isolate,
                                           v8::Local<v8::Value> message) {
  if (!guest_loaded_) {
    pending_messages_.push_back(v8::Global<v8::Value>(isolate, message));
    return;
  }

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(guest_proxy_routing_id_);
  if (!guest_proxy_render_view)
    return;
  blink::WebFrame* guest_proxy_frame =
      guest_proxy_render_view->GetWebView()->MainFrame();
  if (!guest_proxy_frame)
    return;

  v8::Context::Scope context_scope(
      render_frame()->GetWebFrame()->MainWorldScriptContext());

  v8::Local<v8::Object> guest_proxy_window = guest_proxy_frame->GlobalProxy();
  gin::Dictionary window_object(isolate, guest_proxy_window);
  v8::Local<v8::Function> post_message;
  if (!window_object.Get(std::string(kPostMessageName), &post_message))
    return;

  v8::Local<v8::Value> args[] = {
      message,
      // Post the message to any domain inside the browser plugin. The embedder
      // should already know what is embedded.
      gin::StringToV8(isolate, "*")};
  render_frame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      post_message.As<v8::Function>(), guest_proxy_window, arraysize(args),
      args);
}

void MimeHandlerViewContainer::PostMessageFromValue(
    const base::Value& message) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->MainWorldScriptContext());
  PostMessage(isolate, content::V8ValueConverter::Create()->ToV8Value(
                           &message, frame->MainWorldScriptContext()));
}

void MimeHandlerViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(this->element_instance_id(), guest_view::kInstanceIDNone);
  DCHECK_EQ(this->element_instance_id(), element_instance_id);

  if (!render_frame())
    return;

  render_frame()->AttachGuest(element_instance_id);
}

void MimeHandlerViewContainer::OnGuestAttached(int /* unused */,
                                               int guest_proxy_routing_id) {
  // Save the RenderView routing ID of the guest here so it can be used to route
  // PostMessage calls.
  guest_proxy_routing_id_ = guest_proxy_routing_id;
}

void MimeHandlerViewContainer::OnMimeHandlerViewGuestOnLoadCompleted(
    int /* unused */) {
  if (!render_frame())
    return;

  guest_loaded_ = true;
  if (pending_messages_.empty())
    return;

  // Now that the guest has loaded, flush any unsent messages.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->MainWorldScriptContext());
  for (const auto& pending_message : pending_messages_)
    PostMessage(isolate, v8::Local<v8::Value>::New(isolate, pending_message));

  pending_messages_.clear();
}

void MimeHandlerViewContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (guest_created_ || !element_size_.has_value() || view_id_.empty())
    return;

  // The loader has completed loading |view_id_| so we can dispose it.
  if (loader_) {
    DCHECK(is_embedded_);
    loader_.reset();
  }

  DCHECK_NE(element_instance_id(), guest_view::kInstanceIDNone);

  if (!render_frame())
    return;

  render_frame()->Send(
      new ExtensionsGuestViewHostMsg_CreateMimeHandlerViewGuest(
          render_frame()->GetRoutingID(), view_id_, element_instance_id(),
          *element_size_));

  guest_created_ = true;
}

}  // namespace extensions
