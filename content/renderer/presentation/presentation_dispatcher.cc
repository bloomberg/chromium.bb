// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr) {
}

PresentationDispatcher::~PresentationDispatcher() {
  // Controller should be destroyed before the dispatcher when frame is
  // destroyed.
  DCHECK(!controller_);
}

void PresentationDispatcher::setController(
    blink::WebPresentationController* controller) {
  // There shouldn't be any swapping from one non-null controller to another.
  DCHECK(controller != controller_ && (!controller || !controller_));
  controller_ = controller;
  // The controller is set to null when the frame is about to be detached.
  // Nothing is listening for screen availability anymore but the Mojo service
  // will know about the frame being detached anyway.
}

void PresentationDispatcher::updateAvailableChangeWatched(bool watched) {
  ConnectToPresentationServiceIfNeeded();
  if (watched) {
    presentation_service_->GetScreenAvailability(
        mojo::String(),
        base::Bind(&PresentationDispatcher::OnScreenAvailabilityChanged,
                 base::Unretained(this)));
  } else {
    presentation_service_->OnScreenAvailabilityListenerRemoved();
  }
}

void PresentationDispatcher::OnScreenAvailabilityChanged(bool available) {
  if (!controller_)
    return;

  // Reset the callback to get the next event.
  updateAvailableChangeWatched(controller_->isAvailableChangeWatched());

  controller_->didChangeAvailability(available);
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetServiceRegistry()->ConnectToRemoteService(
      &presentation_service_);
}

}  // namespace content
