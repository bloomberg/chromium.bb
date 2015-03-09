// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/extensions_guest_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "extensions/common/guest_view/guest_view_messages.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace {
typedef std::map<int, extensions::ExtensionsGuestViewContainer*>
    ExtensionsGuestViewContainerMap;
static base::LazyInstance<ExtensionsGuestViewContainerMap>
    g_guest_view_container_map = LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace extensions {

ExtensionsGuestViewContainer::Request::Request(
    GuestViewContainer* container,
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate)
    : container_(container), callback_(isolate, callback), isolate_(isolate) {
}

ExtensionsGuestViewContainer::Request::~Request() {
}

bool ExtensionsGuestViewContainer::Request::HasCallback() const {
  return !callback_.IsEmpty();
}

v8::Handle<v8::Function>
ExtensionsGuestViewContainer::Request::GetCallback() const {
  return v8::Local<v8::Function>::New(isolate_, callback_);
}

ExtensionsGuestViewContainer::AttachRequest::AttachRequest(
    GuestViewContainer* container,
    int guest_instance_id,
    scoped_ptr<base::DictionaryValue> params,
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate)
    : Request(container, callback, isolate),
      guest_instance_id_(guest_instance_id),
      params_(params.Pass()) {
}

ExtensionsGuestViewContainer::AttachRequest::~AttachRequest() {
}

void ExtensionsGuestViewContainer::AttachRequest::PerformRequest() {
  if (!container()->render_frame())
    return;

  // Step 1, send the attach params to extensions/.
  container()->render_frame()->Send(
      new GuestViewHostMsg_AttachGuest(container()->element_instance_id(),
                                       guest_instance_id_,
                                       *params_));

  // Step 2, attach plugin through content/.
  container()->render_frame()->AttachGuest(container()->element_instance_id());
}

void ExtensionsGuestViewContainer::AttachRequest::HandleResponse(
    const IPC::Message& message) {
  GuestViewMsg_GuestAttached::Param param;
  if (!GuestViewMsg_GuestAttached::Read(&message, &param))
    return;

  // If we don't have a callback then there's nothing more to do.
  if (!HasCallback())
    return;

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(get<1>(param));
  // TODO(fsamuel): Should we be reporting an error to JavaScript or DCHECKing?
  if (!guest_proxy_render_view)
    return;

  v8::HandleScope handle_scope(isolate());
  v8::Handle<v8::Function> callback = GetCallback();
  v8::Handle<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  blink::WebFrame* frame = guest_proxy_render_view->GetWebView()->mainFrame();
  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();

  const int argc = 1;
  v8::Handle<v8::Value> argv[argc] = { window };

  v8::Context::Scope context_scope(context);
  blink::WebScopedMicrotaskSuppression suppression;

  // Call the AttachGuest API's callback with the guest proxy as the first
  // parameter.
  callback->Call(context->Global(), argc, argv);
}

ExtensionsGuestViewContainer::DetachRequest::DetachRequest(
    GuestViewContainer* container,
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate)
    : Request(container, callback, isolate) {
}

ExtensionsGuestViewContainer::DetachRequest::~DetachRequest() {
}

void ExtensionsGuestViewContainer::DetachRequest::PerformRequest() {
  if (!container()->render_frame())
    return;

  container()->render_frame()->DetachGuest(container()->element_instance_id());
}

void ExtensionsGuestViewContainer::DetachRequest::HandleResponse(
    const IPC::Message& message) {
  // If we don't have a callback then there's nothing more to do.
  if (!HasCallback())
    return;

  v8::HandleScope handle_scope(isolate());
  v8::Handle<v8::Function> callback = GetCallback();
  v8::Handle<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  blink::WebScopedMicrotaskSuppression suppression;

  // Call the DetachGuest's callback.
  callback->Call(context->Global(), 0 /* argc */, nullptr);
}

ExtensionsGuestViewContainer::ExtensionsGuestViewContainer(
    content::RenderFrame* render_frame)
    : GuestViewContainer(render_frame),
      ready_(false),
      destruction_isolate_(nullptr),
      element_resize_isolate_(nullptr),
      weak_ptr_factory_(this) {
}

ExtensionsGuestViewContainer::~ExtensionsGuestViewContainer() {
  if (element_instance_id() != guestview::kInstanceIDNone) {
    g_guest_view_container_map.Get().erase(element_instance_id());
  }

  // Call the destruction callback, if one is registered.
  if (destruction_callback_.IsEmpty())
    return;
  v8::HandleScope handle_scope(destruction_isolate_);
  v8::Handle<v8::Function> callback =
      v8::Local<v8::Function>::New(destruction_isolate_, destruction_callback_);
  v8::Handle<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  blink::WebScopedMicrotaskSuppression suppression;

  callback->Call(context->Global(), 0 /* argc */, nullptr);
}

ExtensionsGuestViewContainer* ExtensionsGuestViewContainer::FromID(
    int element_instance_id) {
  ExtensionsGuestViewContainerMap* guest_view_containers =
      g_guest_view_container_map.Pointer();
  auto it = guest_view_containers->find(element_instance_id);
  return it == guest_view_containers->end() ? nullptr : it->second;
}

void ExtensionsGuestViewContainer::IssueRequest(linked_ptr<Request> request) {
  EnqueueRequest(request);
  PerformPendingRequest();
}

void ExtensionsGuestViewContainer::RegisterDestructionCallback(
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate) {
  destruction_callback_.Reset(isolate, callback);
  destruction_isolate_ = isolate;
}

void ExtensionsGuestViewContainer::RegisterElementResizeCallback(
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate) {
  element_resize_callback_.Reset(isolate, callback);
  element_resize_isolate_ = isolate;
}

void ExtensionsGuestViewContainer::DidResizeElement(const gfx::Size& old_size,
                                                    const gfx::Size& new_size) {
  // Call the element resize callback, if one is registered.
  if (element_resize_callback_.IsEmpty())
    return;

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionsGuestViewContainer::CallElementResizeCallback,
                 weak_ptr_factory_.GetWeakPtr(), old_size, new_size));
}

bool ExtensionsGuestViewContainer::OnMessageReceived(
    const IPC::Message& message) {
  OnHandleCallback(message);
  return true;
}

void ExtensionsGuestViewContainer::SetElementInstanceID(
    int element_instance_id) {
  GuestViewContainer::SetElementInstanceID(element_instance_id);

  DCHECK(g_guest_view_container_map.Get().find(element_instance_id) ==
            g_guest_view_container_map.Get().end());
  g_guest_view_container_map.Get().insert(
      std::make_pair(element_instance_id, this));
}

void ExtensionsGuestViewContainer::Ready() {
  ready_ = true;
  CHECK(!pending_response_.get());
  PerformPendingRequest();
}

void ExtensionsGuestViewContainer::OnHandleCallback(
    const IPC::Message& message) {
  // Handle the callback for the current request with a pending response.
  HandlePendingResponseCallback(message);
  // Perform the subsequent attach request if one exists.
  PerformPendingRequest();
}

void ExtensionsGuestViewContainer::CallElementResizeCallback(
    const gfx::Size& old_size,
    const gfx::Size& new_size) {
  v8::HandleScope handle_scope(element_resize_isolate_);
  v8::Handle<v8::Function> callback = v8::Local<v8::Function>::New(
      element_resize_isolate_, element_resize_callback_);
  v8::Handle<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  const int argc = 4;
  v8::Handle<v8::Value> argv[argc] = {
    v8::Integer::New(element_resize_isolate_, old_size.width()),
    v8::Integer::New(element_resize_isolate_, old_size.height()),
    v8::Integer::New(element_resize_isolate_, new_size.width()),
    v8::Integer::New(element_resize_isolate_, new_size.height())};

  v8::Context::Scope context_scope(context);
  blink::WebScopedMicrotaskSuppression suppression;

  callback->Call(context->Global(), argc, argv);
}

void ExtensionsGuestViewContainer::EnqueueRequest(linked_ptr<Request> request) {
  pending_requests_.push_back(request);
}

void ExtensionsGuestViewContainer::PerformPendingRequest() {
  if (!ready_ || pending_requests_.empty() || pending_response_.get())
    return;

  linked_ptr<Request> pending_request = pending_requests_.front();
  pending_requests_.pop_front();
  pending_request->PerformRequest();
  pending_response_ = pending_request;
}

void ExtensionsGuestViewContainer::HandlePendingResponseCallback(
    const IPC::Message& message) {
  CHECK(pending_response_.get());
  linked_ptr<Request> pending_response(pending_response_.release());
  pending_response->HandleResponse(message);
}

}  // namespace extensions
