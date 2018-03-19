// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_H_

#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "components/ui_devtools/views/ui_element_delegate.h"
#include "ui/aura/env_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ui_devtools {

class UIElement;

class DOMAgentObserver {
 public:
  virtual void OnElementBoundsChanged(UIElement* ui_element) = 0;
};

class DOMAgent : public UiDevToolsBaseAgent<protocol::DOM::Metainfo>,
                 public UIElementDelegate,
                 public aura::EnvObserver {
 public:
  DOMAgent();
  ~DOMAgent() override;

  // DOM::Backend:
  protocol::Response disable() override;
  protocol::Response getDocument(
      std::unique_ptr<protocol::DOM::Node>* out_root) override;
  protocol::Response hideHighlight() override;
  protocol::Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backend_node_ids,
      std::unique_ptr<protocol::Array<int>>* result) override;

  // UIElementDelegate:
  void OnUIElementAdded(UIElement* parent, UIElement* child) override;
  void OnUIElementReordered(UIElement* parent, UIElement* child) override;
  void OnUIElementRemoved(UIElement* ui_element) override;
  void OnUIElementBoundsChanged(UIElement* ui_element) override;

  void AddObserver(DOMAgentObserver* observer);
  void RemoveObserver(DOMAgentObserver* observer);
  UIElement* GetElementFromNodeId(int node_id);
  UIElement* element_root() const { return element_root_.get(); };
  const std::vector<gfx::NativeWindow>& root_windows() const {
    return root_windows_;
  };

  // Returns parent id of the element with id |node_id|. Returns 0 if parent
  // does not exist.
  int GetParentIdOfNodeId(int node_id) const;

 private:

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  void OnElementBoundsChanged(UIElement* ui_element);
  std::unique_ptr<protocol::DOM::Node> BuildInitialTree();
  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root,
      aura::Window* window);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForRootWidget(
      UIElement* widget_element,
      views::Widget* widget);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForView(UIElement* view_element,
                                                        views::View* view);
  void RemoveDomNode(UIElement* ui_element);
  void Reset();

  bool is_building_tree_;
  std::unique_ptr<UIElement> element_root_;
  std::unordered_map<int, UIElement*> node_id_to_ui_element_;

  std::vector<aura::Window*> root_windows_;

  base::ObserverList<DOMAgentObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_UI_DOM_AGENT_H_
