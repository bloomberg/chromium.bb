// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/guest_view_constants.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

MimeHandlerViewContainer::MimeHandlerViewContainer(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url)
    : GuestViewContainer(render_frame),
      mime_type_(mime_type),
      original_url_(original_url) {
  DCHECK(!mime_type_.empty());
  is_embedded_ = !render_frame->GetWebFrame()->document().isPluginDocument();
}

MimeHandlerViewContainer::~MimeHandlerViewContainer() {
  if (loader_)
    loader_->cancel();
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

void MimeHandlerViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(this->element_instance_id(), guestview::kInstanceIDNone);
  DCHECK_EQ(this->element_instance_id(), element_instance_id);
  render_frame()->AttachGuest(element_instance_id);
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
