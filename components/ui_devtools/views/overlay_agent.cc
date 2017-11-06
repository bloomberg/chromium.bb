// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/overlay_agent.h"

#include "ui/aura/env.h"
#include "ui/events/event.h"

namespace ui_devtools {

OverlayAgent::OverlayAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

OverlayAgent::~OverlayAgent() {}

void OverlayAgent::SetPinnedNodeId(int node_id) {
  pinned_id_ = node_id;
  frontend()->nodeHighlightRequested(pinned_id_);
  dom_agent_->HighlightNode(pinned_id_, true /* show_size */);
}

protocol::Response OverlayAgent::setInspectMode(
    const String& in_mode,
    protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  pinned_id_ = 0;
  if (in_mode.compare("searchForNode") == 0)
    aura::Env::GetInstance()->PrependPreTargetHandler(this);
  else if (in_mode.compare("none") == 0)
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  return protocol::Response::OK();
}

protocol::Response OverlayAgent::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
    protocol::Maybe<int> node_id) {
  return dom_agent_->HighlightNode(node_id.fromJust());
}

protocol::Response OverlayAgent::hideHighlight() {
  return dom_agent_->hideHighlight();
}

void OverlayAgent::OnMouseEvent(ui::MouseEvent* event) {
  // Make sure the element tree has been populated before processing
  // mouse events.
  if (!dom_agent_->window_element_root())
    return;

  // Show parent of the pinned element with id |pinned_id_| when mouse scrolls
  // up. If parent exists, hightlight and re-pin parent element.
  if (event->type() == ui::ET_MOUSEWHEEL && pinned_id_) {
    const ui::MouseWheelEvent* mouse_event =
        static_cast<ui::MouseWheelEvent*>(event);
    DCHECK(mouse_event);
    if (mouse_event->y_offset() > 0) {
      const int parent_node_id = dom_agent_->GetParentIdOfNodeId(pinned_id_);
      if (parent_node_id)
        SetPinnedNodeId(parent_node_id);
    } else if (mouse_event->y_offset() < 0) {
      // TODO(thanhph): discuss behaviours when mouse scrolls down.
    }
    return;
  }

  // Find node id of element whose bounds contain the mouse pointer location.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  int element_id = dom_agent_->FindElementIdTargetedByPoint(
      event->root_location(), target->GetRootWindow());

  if (pinned_id_ == element_id) {
    event->SetHandled();
    return;
  }

  // Pin the hover element on click.
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    event->SetHandled();
    if (element_id)
      SetPinnedNodeId(element_id);
  } else if (element_id && !pinned_id_) {
    // Display only guidelines if hovering without a pinned element.
    frontend()->nodeHighlightRequested(element_id);
    dom_agent_->HighlightNode(element_id, false /* show_size */);
  } else if (element_id && pinned_id_) {
    // If hovering with a pinned element, then show distances between the pinned
    // element and the hover element.
    dom_agent_->HighlightNode(element_id, false /* show_size */);
    dom_agent_->ShowDistancesInHighlightOverlay(pinned_id_, element_id);
  }
}

void OverlayAgent::OnKeyEvent(ui::KeyEvent* event) {
  if (!dom_agent_->window_element_root())
    return;

  // Exit inspect mode by pressing ESC key.
  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
    if (pinned_id_) {
      frontend()->inspectNodeRequested(pinned_id_);
      dom_agent_->HighlightNode(pinned_id_, true /* show_size */);
    }
    // Unpin element.
    pinned_id_ = 0;
  }
}

}  // namespace ui_devtools
