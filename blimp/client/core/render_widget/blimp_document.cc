// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/render_widget/blimp_document.h"

#include "blimp/client/core/render_widget/blimp_document_manager.h"
#include "cc/layers/layer.h"

namespace blimp {
namespace client {

BlimpDocument::BlimpDocument(int document_id,
                             BlimpCompositorDependencies* compositor_deps,
                             BlimpDocumentManager* document_manager)
    : document_id_(document_id), manager_(document_manager) {
  compositor_ = base::MakeUnique<BlimpCompositor>(compositor_deps, this);
}

BlimpDocument::~BlimpDocument() = default;

BlimpCompositor* BlimpDocument::GetCompositor() {
  return compositor_.get();
}

void BlimpDocument::SetCompositorForTest(
    std::unique_ptr<BlimpCompositor> compositor) {
  compositor_ = std::move(compositor);
}

void BlimpDocument::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  manager_->SendWebGestureEvent(document_id_, gesture_event);
}

void BlimpDocument::SendCompositorMessage(
    const cc::proto::CompositorMessage& message) {
  manager_->SendCompositorMessage(document_id_, message);
}

}  // namespace client
}  // namespace blimp
