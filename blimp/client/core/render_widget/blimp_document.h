// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_H_
#define BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/compositor/blimp_compositor.h"

namespace blimp {
namespace client {

class BlimpCompositorDependencies;
class BlimpDocumentManager;

// BlimpDocument maps to an engine side render widget.
// 1. Is created on receiving an RenderWidgetMessage from the engine.
// 2. Owns the BlimpCompositor instance.
class BlimpDocument : public BlimpCompositorClient {
 public:
  BlimpDocument(int document_id,
                BlimpCompositorDependencies* compositor_dependencies,
                BlimpDocumentManager* document_manager);
  ~BlimpDocument() override;

  int document_id() const { return document_id_; }

  // Returns the compositor instance.
  BlimpCompositor* GetCompositor();

 protected:
  void SetCompositorForTest(std::unique_ptr<BlimpCompositor> compositor);

 private:
  // BlimpCompositorClient implementation.
  void SendWebGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  void SendCompositorMessage(
      const cc::proto::CompositorMessage& message) override;

  // The unique identifier for this document instance.
  const int document_id_;

  // The compositor instance.
  std::unique_ptr<BlimpCompositor> compositor_;

  // Used to send messages to the corresponding render widget on the engine.
  BlimpDocumentManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(BlimpDocument);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_H_
