// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcolorchooser_impl.h"

#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

RendererWebColorChooserImpl::RendererWebColorChooserImpl(
    RenderFrame* render_frame,
    blink::WebColorChooserClient* blink_client)
    : RenderFrameObserver(render_frame),
      blink_client_(blink_client),
      mojo_client_binding_(this) {
  render_frame->GetRemoteInterfaces()->GetInterface(&color_chooser_factory_);
}

RendererWebColorChooserImpl::~RendererWebColorChooserImpl() {
}

void RendererWebColorChooserImpl::SetSelectedColor(blink::WebColor color) {
  color_chooser_->SetSelectedColor(color);
}

void RendererWebColorChooserImpl::EndChooser() {
  color_chooser_.reset();
}

void RendererWebColorChooserImpl::Open(
    SkColor initial_color,
    std::vector<mojom::ColorSuggestionPtr> suggestions) {
  content::mojom::ColorChooserClientPtr mojo_client;
  mojo_client_binding_.Bind(mojo::MakeRequest(&mojo_client));
  mojo_client_binding_.set_connection_error_handler(base::BindOnce(
      [](blink::WebColorChooserClient* blink_client) {
        blink_client->DidEndChooser();
      },
      base::Unretained(blink_client_)));
  color_chooser_factory_->OpenColorChooser(
      mojo::MakeRequest(&color_chooser_), std::move(mojo_client), initial_color,
      std::move(suggestions));
}

void RendererWebColorChooserImpl::DidChooseColor(SkColor color) {
  blink_client_->DidChooseColor(static_cast<blink::WebColor>(color));
}

}  // namespace content
