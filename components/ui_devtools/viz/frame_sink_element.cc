// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz/frame_sink_element.h"

#include "base/strings/string_piece.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace ui_devtools {

FrameSinkElement::FrameSinkElement(
    const viz::FrameSinkId& frame_sink_id,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    UIElementDelegate* ui_element_delegate,
    UIElement* parent,
    bool is_root,
    bool has_created_frame_sink)
    : UIElement(UIElementType::FRAMESINK, ui_element_delegate, parent),
      frame_sink_id_(frame_sink_id),
      frame_sink_manager_(frame_sink_manager),
      is_root_(is_root),
      has_created_frame_sink_(has_created_frame_sink) {
  // DOMAgentViz handles all of the FrameSink events, so it owns all of the
  // FrameSinkElements.
  set_owns_children(false);
}

FrameSinkElement::~FrameSinkElement() {}

std::vector<std::pair<std::string, std::string>>
FrameSinkElement::GetCustomProperties() const {
  std::vector<std::pair<std::string, std::string>> v;

  // Hierarchical information about the FrameSink.
  v.push_back(std::make_pair("Is root", is_root_ ? "true" : "false"));
  v.push_back(std::make_pair("Has created frame sink",
                             has_created_frame_sink_ ? "true" : "false"));

  // LastUsedBeingFrameArgs information.
  const viz::CompositorFrameSinkSupport* support =
      frame_sink_manager_->GetFrameSinkForId(frame_sink_id_);
  if (support) {
    const viz::BeginFrameArgs args =
        static_cast<const viz::BeginFrameObserver*>(support)
            ->LastUsedBeginFrameArgs();
    v.push_back(std::make_pair("SourceId", std::to_string(args.source_id)));
    v.push_back(
        std::make_pair("SequenceNumber", std::to_string(args.sequence_number)));
    v.push_back(std::make_pair(
        "FrameType",
        std::string(viz::BeginFrameArgs::TypeToString(args.type))));
  }
  return v;
}

void FrameSinkElement::GetBounds(gfx::Rect* bounds) const {
  const viz::CompositorFrameSinkSupport* support =
      frame_sink_manager_->GetFrameSinkForId(frame_sink_id_);
  if (!support) {
    *bounds = gfx::Rect();
    return;
  }

  // Get just size of last activated surface that corresponds to this frame
  // sink.
  viz::Surface* srfc = frame_sink_manager_->surface_manager()->GetSurfaceForId(
      support->last_activated_surface_id());
  if (srfc)
    *bounds = gfx::Rect(srfc->size_in_pixels());
  else
    *bounds = gfx::Rect();
}

void FrameSinkElement::SetBounds(const gfx::Rect& bounds) {}

void FrameSinkElement::GetVisible(bool* visible) const {
  // Currently not real data.
  *visible = true;
}

void FrameSinkElement::SetVisible(bool visible) {}

std::unique_ptr<protocol::Array<std::string>> FrameSinkElement::GetAttributes()
    const {
  auto attributes = protocol::Array<std::string>::create();
  attributes->addItem("FrameSinkId");
  attributes->addItem(frame_sink_id_.ToString());
  attributes->addItem("Title");
  attributes->addItem(
      (frame_sink_manager_->GetFrameSinkDebugLabel(frame_sink_id_)).data());
  return attributes;
}

std::pair<gfx::NativeWindow, gfx::Rect>
FrameSinkElement::GetNodeWindowAndScreenBounds() const {
  return {};
}

// static
const viz::FrameSinkId& FrameSinkElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::FRAMESINK, element->type());
  return static_cast<const FrameSinkElement*>(element)->frame_sink_id_;
}

}  // namespace ui_devtools
