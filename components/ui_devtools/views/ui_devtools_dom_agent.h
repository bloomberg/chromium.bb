// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_DOM_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_DOM_AGENT_H_

#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "components/ui_devtools/views/ui_element_delegate.h"
#include "ui/aura/env_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace gfx {
class RenderText;
}

namespace ui_devtools {

enum HighlightRectsConfiguration {
  NO_DRAW,
  R1_CONTAINS_R2,
  R1_HORIZONTAL_FULL_LEFT_R2,
  R1_TOP_FULL_LEFT_R2,
  R1_BOTTOM_FULL_LEFT_R2,
  R1_TOP_PARTIAL_LEFT_R2,
  R1_BOTTOM_PARTIAL_LEFT_R2,
  R1_INTERSECTS_R2
};

enum RectSide { TOP_SIDE, LEFT_SIDE, RIGHT_SIDE, BOTTOM_SIDE };

class UIElement;

class UIDevToolsDOMAgentObserver {
 public:
  virtual void OnElementBoundsChanged(UIElement* ui_element) = 0;
};

class UIDevToolsDOMAgent : public ui_devtools::UiDevToolsBaseAgent<
                               ui_devtools::protocol::DOM::Metainfo>,
                           public UIElementDelegate,
                           public aura::EnvObserver,
                           public ui::LayerDelegate {
 public:
  UIDevToolsDOMAgent();
  ~UIDevToolsDOMAgent() override;

  // DOM::Backend:
  ui_devtools::protocol::Response disable() override;
  ui_devtools::protocol::Response getDocument(
      std::unique_ptr<ui_devtools::protocol::DOM::Node>* out_root) override;
  ui_devtools::protocol::Response hideHighlight() override;
  ui_devtools::protocol::Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backend_node_ids,
      std::unique_ptr<protocol::Array<int>>* result) override;

  // UIElementDelegate:
  void OnUIElementAdded(UIElement* parent, UIElement* child) override;
  void OnUIElementReordered(UIElement* parent, UIElement* child) override;
  void OnUIElementRemoved(UIElement* ui_element) override;
  void OnUIElementBoundsChanged(UIElement* ui_element) override;

  void AddObserver(UIDevToolsDOMAgentObserver* observer);
  void RemoveObserver(UIDevToolsDOMAgentObserver* observer);
  UIElement* GetElementFromNodeId(int node_id);
  UIElement* window_element_root() const { return window_element_root_.get(); };
  const std::vector<aura::Window*>& root_windows() const {
    return root_windows_;
  };
  HighlightRectsConfiguration highlight_rect_config() const {
    return highlight_rect_config_;
  };
  ui_devtools::protocol::Response HighlightNode(int node_id,
                                                bool show_size = false);

  // Return the id of the UI element targeted by an event located at |p|, where
  // |p| is in the local coodinate space of |root_window|. The function
  // first searches for the targeted window, then the targeted widget (if one
  // exists), then the targeted view (if one exists). Return 0 if no valid
  // target is found.
  int FindElementIdTargetedByPoint(const gfx::Point& p,
                                   aura::Window* root_window) const;

  // Shows the distances between the nodes identified by |pinned_id| and
  // |element_id| in the highlight overlay.
  void ShowDistancesInHighlightOverlay(int pinned_id, int element_id);

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  void OnElementBoundsChanged(UIElement* ui_element);
  std::unique_ptr<ui_devtools::protocol::DOM::Node> BuildInitialTree();
  std::unique_ptr<ui_devtools::protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element);
  std::unique_ptr<ui_devtools::protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root,
      aura::Window* window);
  std::unique_ptr<ui_devtools::protocol::DOM::Node> BuildTreeForRootWidget(
      UIElement* widget_element,
      views::Widget* widget);
  std::unique_ptr<ui_devtools::protocol::DOM::Node> BuildTreeForView(
      UIElement* view_element,
      views::View* view);
  void RemoveDomNode(UIElement* ui_element);
  void Reset();
  void UpdateHighlight(
      const std::pair<aura::Window*, gfx::Rect>& window_and_bounds);

  std::unique_ptr<gfx::RenderText> render_text_;
  bool is_building_tree_;
  bool show_size_on_canvas_ = false;
  HighlightRectsConfiguration highlight_rect_config_;
  bool is_swap_ = false;
  std::unique_ptr<UIElement> window_element_root_;
  std::unordered_map<int, UIElement*> node_id_to_ui_element_;

  // TODO(thanhph): |layer_for_highlighting_| should be owned by the overlay
  // agent.
  std::unique_ptr<ui::Layer> layer_for_highlighting_;
  std::vector<aura::Window*> root_windows_;
  gfx::Rect hovered_rect_;
  gfx::Rect pinned_rect_;
  base::ObserverList<UIDevToolsDOMAgentObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(UIDevToolsDOMAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DEVTOOLS_DOM_AGENT_H_
