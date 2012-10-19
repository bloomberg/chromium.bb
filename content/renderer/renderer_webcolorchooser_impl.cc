// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcolorchooser_impl.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"

namespace content {

static int GenerateColorChooserIdentifier() {
  static int next = 0;
  return ++next;
}

RendererWebColorChooserImpl::RendererWebColorChooserImpl(
    RenderViewImpl* render_view,
    WebKit::WebColorChooserClient* client)
    : RenderViewObserver(render_view),
      identifier_(GenerateColorChooserIdentifier()),
      client_(client) {
}

RendererWebColorChooserImpl::~RendererWebColorChooserImpl() {
}

bool RendererWebColorChooserImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererWebColorChooserImpl, message)
    IPC_MESSAGE_HANDLER(ViewMsg_DidChooseColorResponse,
                        OnDidChooseColorResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_DidEndColorChooser,
                        OnDidEndColorChooser)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererWebColorChooserImpl::setSelectedColor(WebKit::WebColor color) {
  Send(new ViewHostMsg_SetSelectedColorInColorChooser(routing_id(), identifier_,
      static_cast<SkColor>(color)));
}

void RendererWebColorChooserImpl::endChooser() {
  Send(new ViewHostMsg_EndColorChooser(routing_id(), identifier_));
}

void RendererWebColorChooserImpl::Open(SkColor initial_color) {
  Send(new ViewHostMsg_OpenColorChooser(routing_id(), identifier_,
                                        initial_color));
}

void RendererWebColorChooserImpl::OnDidChooseColorResponse(int color_chooser_id,
                                                           SkColor color) {
  DCHECK(identifier_ == color_chooser_id);

  client_->didChooseColor(static_cast<WebKit::WebColor>(color));
}

void RendererWebColorChooserImpl::OnDidEndColorChooser(int color_chooser_id) {
  if (identifier_ != color_chooser_id)
    return;
  client_->didEndChooser();
}

}  // namespace content
