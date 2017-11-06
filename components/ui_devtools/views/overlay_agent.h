// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_H_

#include "components/ui_devtools/Overlay.h"
#include "components/ui_devtools/views/dom_agent.h"
#include "ui/events/event_handler.h"

namespace ui_devtools {

class OverlayAgent : public UiDevToolsBaseAgent<protocol::Overlay::Metainfo>,
                     public ui::EventHandler {
 public:
  explicit OverlayAgent(DOMAgent* dom_agent);
  ~OverlayAgent() override;
  int pinned_id() const { return pinned_id_; };
  void SetPinnedNodeId(int pinned_id);

  // Overlay::Backend:
  protocol::Response setInspectMode(
      const String& in_mode,
      protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig)
      override;
  protocol::Response highlightNode(
      std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
      protocol::Maybe<int> node_id) override;
  protocol::Response hideHighlight() override;

 private:
  // ui:EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  DOMAgent* const dom_agent_;
  int pinned_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OverlayAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_OVERLAY_AGENT_H_
