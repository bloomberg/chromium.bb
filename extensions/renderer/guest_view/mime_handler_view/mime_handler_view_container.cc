// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
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

  static v8::Handle<v8::Object> Create(
      v8::Isolate* isolate,
      base::WeakPtr<MimeHandlerViewContainer> container) {
    ScriptableObject* scriptable_object =
        new ScriptableObject(isolate, container);
    return gin::CreateHandle(isolate, scriptable_object).ToV8()->ToObject();
  }

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(
      v8::Isolate* isolate,
      const std::string& identifier) override {
    if (identifier == kPostMessageName) {
      return gin::CreateFunctionTemplate(isolate,
          base::Bind(&MimeHandlerViewContainer::PostMessage,
                     container_, isolate))->GetFunction();
    }
    return v8::Local<v8::Value>();
  }

 private:
  ScriptableObject(v8::Isolate* isolate,
                   base::WeakPtr<MimeHandlerViewContainer> container)
    : gin::NamedPropertyInterceptor(isolate, this),
      container_(container) {}
  virtual ~ScriptableObject() {}

  // gin::Wrappable
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<ScriptableObject>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor();
  }

  base::WeakPtr<MimeHandlerViewContainer> container_;
};

// static
gin::WrapperInfo ScriptableObject::kWrapperInfo = { gin::kEmbedderNativeGin };

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
  is_embedded_ = !render_frame->GetWebFrame()->document().isPluginDocument();
}

MimeHandlerViewContainer::~MimeHandlerViewContainer() {
  if (loader_)
    loader_->cancel();
}

void MimeHandlerViewContainer::Ready() {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  blink::WebURLLoaderOptions options;
  // The embedded plugin is allowed to be cross-origin.
  options.crossOriginRequestPolicy =
      blink::WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
  DCHECK(!loader_);
  loader_.reset(frame->createAssociatedURLLoader(options));

  blink::WebURLRequest request(original_url_);
  request.setRequestContext(blink::WebURLRequest::RequestContextObject);
  loader_->loadAsynchronously(request, this);
}

void MimeHandlerViewContainer::DidFinishLoading() {
  DCHECK(!is_embedded_);
  CreateMimeHandlerViewGuest();
}

void MimeHandlerViewContainer::DidReceiveData(const char* data,
                                              int data_length) {
  html_string_ += std::string(data, data_length);
}

bool MimeHandlerViewContainer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeHandlerViewContainer, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GuestAttached, OnGuestAttached)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MimeHandlerViewGuestOnLoadCompleted,
                        OnMimeHandlerViewGuestOnLoadCompleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

v8::Local<v8::Object> MimeHandlerViewContainer::V8ScriptableObject(
    v8::Isolate* isolate) {
  if (scriptable_object_.IsEmpty()) {
    v8::Local<v8::Object> object =
        ScriptableObject::Create(isolate, weak_factory_.GetWeakPtr());
    scriptable_object_.reset(object);
  }
  return scriptable_object_.NewHandle(isolate);
}

void MimeHandlerViewContainer::didReceiveData(blink::WebURLLoader* /* unused */,
                                              const char* data,
                                              int data_length,
                                              int /* unused */) {
  html_string_ += std::string(data, data_length);
}

void MimeHandlerViewContainer::didFinishLoading(
    blink::WebURLLoader* /* unused */,
    double /* unused */,
    int64_t /* unused */) {
  DCHECK(is_embedded_);
  CreateMimeHandlerViewGuest();
}

void MimeHandlerViewContainer::PostMessage(v8::Isolate* isolate,
                                           v8::Handle<v8::Value> message) {
  if (!guest_loaded_) {
    linked_ptr<ScopedPersistent<v8::Value>> scoped_persistent(
        new ScopedPersistent<v8::Value>(isolate, message));
    pending_messages_.push_back(scoped_persistent);
    return;
  }

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(guest_proxy_routing_id_);
  if (!guest_proxy_render_view)
    return;
  blink::WebFrame* guest_proxy_frame =
      guest_proxy_render_view->GetWebView()->mainFrame();
  if (!guest_proxy_frame)
    return;

  v8::Local<v8::Object> guest_proxy_window =
      guest_proxy_frame->mainWorldScriptContext()->Global();
  gin::Dictionary window_object(isolate, guest_proxy_window);
  v8::Handle<v8::Function> post_message;
  if (!window_object.Get(std::string(kPostMessageName), &post_message))
    return;

  v8::Handle<v8::Value> args[] = {
    message,
    // Post the message to any domain inside the browser plugin. The embedder
    // should already know what is embedded.
    gin::StringToV8(isolate, "*")
  };
  post_message.As<v8::Function>()->Call(
      guest_proxy_window, arraysize(args), args);
}

void MimeHandlerViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(this->element_instance_id(), guestview::kInstanceIDNone);
  DCHECK_EQ(this->element_instance_id(), element_instance_id);
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
  guest_loaded_ = true;
  if (pending_messages_.empty())
    return;

  // Now that the guest has loaded, flush any unsent messages.
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->mainWorldScriptContext());
  for (const auto& pending_message : pending_messages_)
    PostMessage(isolate, pending_message->NewHandle(isolate));

  pending_messages_.clear();
}

void MimeHandlerViewContainer::CreateMimeHandlerViewGuest() {
  // The loader has completed loading |html_string_| so we can dispose it.
  loader_.reset();

  // Parse the stream URL to ensure it's valid.
  GURL stream_url(html_string_);

  DCHECK_NE(element_instance_id(), guestview::kInstanceIDNone);
  render_frame()->Send(new ExtensionHostMsg_CreateMimeHandlerViewGuest(
      render_frame()->GetRoutingID(), stream_url.spec(), original_url_.spec(),
      mime_type_, element_instance_id()));
}

}  // namespace extensions
