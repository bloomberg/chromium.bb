// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_devtools_overlay_agent.h"

#include "ui/aura/env.h"

namespace ui_devtools {

UIDevToolsOverlayAgent::UIDevToolsOverlayAgent(UIDevToolsDOMAgent* dom_agent)
    : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

UIDevToolsOverlayAgent::~UIDevToolsOverlayAgent() {}

ui_devtools::protocol::Response UIDevToolsOverlayAgent::setInspectMode(
    const String& in_mode,
    protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  pinned_id_ = 0;
  if (in_mode.compare("searchForNode") == 0)
    aura::Env::GetInstance()->PrependPreTargetHandler(this);
  else if (in_mode.compare("none") == 0)
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  return ui_devtools::protocol::Response::OK();
}

ui_devtools::protocol::Response UIDevToolsOverlayAgent::highlightNode(
    std::unique_ptr<ui_devtools::protocol::Overlay::HighlightConfig>
        highlight_config,
    ui_devtools::protocol::Maybe<int> node_id) {
  return dom_agent_->HighlightNode(node_id.fromJust());
}

ui_devtools::protocol::Response UIDevToolsOverlayAgent::hideHighlight() {
  return dom_agent_->hideHighlight();
}

void UIDevToolsOverlayAgent::OnMouseEvent(ui::MouseEvent* event) {
  // Make sure the element tree has been populated before processing
  // mouse events.
  if (!dom_agent_->window_element_root())
    return;

  // Find node id of element whose bounds contain the mouse pointer location.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  int element_id = dom_agent_->FindElementIdTargetedByPoint(
      event->root_location(), target->GetRootWindow());

  if (event->type() == ui::ET_MOUSE_PRESSED && (pinned_id_ != element_id)) {
    event->SetHandled();
    if (element_id) {
      pinned_id_ = element_id;
      frontend()->nodeHighlightRequested(element_id);
      dom_agent_->HighlightNode(element_id, true);
    }
  } else if (element_id && !pinned_id_) {
    frontend()->nodeHighlightRequested(element_id);
    dom_agent_->HighlightNode(element_id, false);
  }
}

void UIDevToolsOverlayAgent::OnKeyEvent(ui::KeyEvent* event) {
  if (!dom_agent_->window_element_root())
    return;

  // Exit inspection mode by pressing ESC key.
  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
    if (pinned_id_) {
      frontend()->inspectNodeRequested(pinned_id_);
      dom_agent_->HighlightNode(pinned_id_, true);
    }
    pinned_id_ = 0;
  }
}

}  // namespace ui_devtools
