// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/modules/presentation/web_presentation_receiver.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), receiver_(nullptr) {}

PresentationDispatcher::~PresentationDispatcher() {
}

void PresentationDispatcher::SetReceiver(
    blink::WebPresentationReceiver* receiver) {
  receiver_ = receiver;

  DCHECK(!render_frame() || render_frame()->IsMainFrame());

  // Init |receiver_| after loading document.
  if (receiver_ && render_frame() && render_frame()->GetWebFrame() &&
      !render_frame()->GetWebFrame()->IsLoading()) {
    receiver_->Init();
  }
}

void PresentationDispatcher::DidFinishDocumentLoad() {
  if (receiver_)
    receiver_->Init();
}

void PresentationDispatcher::OnDestruct() {
  delete this;
}

void PresentationDispatcher::WidgetWillClose() {
  if (receiver_)
    receiver_->OnReceiverTerminated();
}

}  // namespace content
