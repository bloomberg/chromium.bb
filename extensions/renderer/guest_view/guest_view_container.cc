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

GuestViewContainer::GuestViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type)
    : content::BrowserPluginDelegate(render_frame, mime_type),
      content::RenderFrameObserver(render_frame),
      mime_type_(mime_type),
      element_instance_id_(guestview::kInstanceIDNone),
      render_view_routing_id_(render_frame->GetRenderView()->GetRoutingID()),
      attached_(false),
      attach_pending_(false),
      isolate_(NULL) {
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


void GuestViewContainer::AttachGuest(int element_instance_id,
                                     int guest_instance_id,
                                     scoped_ptr<base::DictionaryValue> params,
                                     v8::Handle<v8::Function> callback,
                                     v8::Isolate* isolate) {
  // GuestViewContainer supports reattachment (i.e. attached_ == true) but not
  // while a current attach process is pending.
  if (attach_pending_)
    return;

  // Step 1, send the attach params to chrome/.
  render_frame()->Send(new ExtensionHostMsg_AttachGuest(render_view_routing_id_,
                                                        element_instance_id,
                                                        guest_instance_id,
                                                        *params));

  // Step 2, attach plugin through content/.
  render_frame()->AttachGuest(element_instance_id);

  callback_.reset(callback);
  isolate_ = isolate;
  attach_pending_ = true;
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
                                         int guest_routing_id) {
  attached_ = true;
  attach_pending_ = false;

  // If we don't have a callback then there's nothing more to do.
  if (callback_.IsEmpty())
    return;

  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(guest_routing_id);
  // TODO(fsamuel): Should we be reporting an error to JavaScript or DCHECKing?
  if (!guest_proxy_render_view)
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Handle<v8::Function> callback = callback_.NewHandle(isolate_);
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
  callback_.reset();
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
