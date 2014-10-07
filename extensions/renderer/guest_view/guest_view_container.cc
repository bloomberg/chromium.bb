// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_container.h"

#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace {
typedef std::pair<int, int> GuestViewID;
typedef std::map<GuestViewID, extensions::GuestViewContainer*>
    GuestViewContainerMap;
static base::LazyInstance<GuestViewContainerMap> g_guest_view_container_map =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace extensions {

GuestViewContainer::AttachRequest::AttachRequest(
    int element_instance_id,
    int guest_instance_id,
    scoped_ptr<base::DictionaryValue> params,
    v8::Handle<v8::Function> callback,
    v8::Isolate* isolate)
    : element_instance_id_(element_instance_id),
      guest_instance_id_(guest_instance_id),
      params_(params.Pass()),
      callback_(callback),
      isolate_(isolate) {
}

GuestViewContainer::AttachRequest::~AttachRequest() {
}

bool GuestViewContainer::AttachRequest::HasCallback() const {
  return !callback_.IsEmpty();
}

v8::Handle<v8::Function>
GuestViewContainer::AttachRequest::GetCallback() const {
  return callback_.NewHandle(isolate_);
}

GuestViewContainer::GuestViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type)
    : content::BrowserPluginDelegate(render_frame, mime_type),
      content::RenderFrameObserver(render_frame),
      mime_type_(mime_type),
      element_instance_id_(guestview::kInstanceIDNone),
      render_view_routing_id_(render_frame->GetRenderView()->GetRoutingID()),
      attached_(false),
      ready_(false) {
}

GuestViewContainer::~GuestViewContainer() {
  if (element_instance_id_ != guestview::kInstanceIDNone) {
    g_guest_view_container_map.Get().erase(
        GuestViewID(render_view_routing_id_, element_instance_id_));
  }
}

GuestViewContainer* GuestViewContainer::FromID(int render_view_routing_id,
                                               int element_instance_id) {
  GuestViewContainerMap* guest_view_containers =
      g_guest_view_container_map.Pointer();
  GuestViewContainerMap::iterator it = guest_view_containers->find(
      GuestViewID(render_view_routing_id, element_instance_id));
  return it == guest_view_containers->end() ? NULL : it->second;
}

void GuestViewContainer::AttachGuest(linked_ptr<AttachRequest> request) {
  EnqueueAttachRequest(request);
  PerformPendingAttachRequest();
}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  GuestViewID guest_view_id(render_view_routing_id_, element_instance_id);
  DCHECK_EQ(element_instance_id_, guestview::kInstanceIDNone);
  DCHECK(g_guest_view_container_map.Get().find(guest_view_id) ==
            g_guest_view_container_map.Get().end());
  element_instance_id_ = element_instance_id;
  g_guest_view_container_map.Get().insert(std::make_pair(guest_view_id, this));
}

void GuestViewContainer::DidFinishLoading() {
  if (mime_type_.empty())
    return;

  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  render_frame()->Send(new ExtensionHostMsg_CreateMimeHandlerViewGuest(
      routing_id(), html_string_, mime_type_, element_instance_id_));
}

void GuestViewContainer::DidReceiveData(const char* data, int data_length) {
  std::string value(data, data_length);
  html_string_ += value;
}

void GuestViewContainer::Ready() {
  ready_ = true;
  CHECK(!pending_response_.get());
  PerformPendingAttachRequest();
}

void GuestViewContainer::OnDestruct() {
  // GuestViewContainer's lifetime is managed by BrowserPlugin so don't let
  // RenderFrameObserver self-destruct here.
}

bool GuestViewContainer::OnMessageReceived(const IPC::Message& message) {
  if (!ShouldHandleMessage(message))
    return false;

  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  int element_instance_id = guestview::kInstanceIDNone;
  PickleIterator iter(message);
  bool success = iter.ReadInt(&element_instance_id);
  DCHECK(success);
  if (element_instance_id != element_instance_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GuestViewContainer, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GuestAttached, OnGuestAttached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GuestViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(element_instance_id_, guestview::kInstanceIDNone);
  DCHECK_EQ(element_instance_id_, element_instance_id);
  DCHECK(!mime_type_.empty());
  render_frame()->AttachGuest(element_instance_id);
}

void GuestViewContainer::OnGuestAttached(int element_instance_id,
                                         int guest_proxy_routing_id) {
  attached_ = true;

  if (!mime_type_.empty()) {
    // MimeHandlerView's creation and attachment is not done via JS API.
    return;
  }

  // Handle the callback for the current request with a pending response.
  HandlePendingResponseCallback(guest_proxy_routing_id);
  // Perform the subsequent attach request if one exists.
  PerformPendingAttachRequest();
}

void GuestViewContainer::AttachGuestInternal(
    linked_ptr<AttachRequest> request) {
  CHECK(!pending_response_.get());
  // Step 1, send the attach params to chrome/.
  render_frame()->Send(
      new ExtensionHostMsg_AttachGuest(render_view_routing_id_,
                                       request->element_instance_id(),
                                       request->guest_instance_id(),
                                       *request->attach_params()));

  // Step 2, attach plugin through content/.
  render_frame()->AttachGuest(request->element_instance_id());

  pending_response_ = request;
}

void GuestViewContainer::EnqueueAttachRequest(
    linked_ptr<AttachRequest> request) {
  pending_requests_.push_back(request);
}

void GuestViewContainer::PerformPendingAttachRequest() {
  if (!ready_ || pending_requests_.empty() || pending_response_.get())
    return;

  linked_ptr<AttachRequest> pending_request = pending_requests_.front();
  pending_requests_.pop_front();
  AttachGuestInternal(pending_request);
}

void GuestViewContainer::HandlePendingResponseCallback(
    int guest_proxy_routing_id) {
  CHECK(pending_response_.get());
  linked_ptr<AttachRequest> pending_response(pending_response_.release());

  // If we don't have a callback then there's nothing more to do.
  if (!pending_response->HasCallback())
    return;

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(guest_proxy_routing_id);
  // TODO(fsamuel): Should we be reporting an error to JavaScript or DCHECKing?
  if (!guest_proxy_render_view)
    return;

  v8::HandleScope handle_scope(pending_response->isolate());
  v8::Handle<v8::Function> callback = pending_response->GetCallback();
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

// static
bool GuestViewContainer::ShouldHandleMessage(const IPC::Message& message) {
  switch (message.type()) {
    case ExtensionMsg_CreateMimeHandlerViewGuestACK::ID:
    case ExtensionMsg_GuestAttached::ID:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace extensions
