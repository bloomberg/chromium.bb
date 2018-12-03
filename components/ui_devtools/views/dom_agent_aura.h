// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_

#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/dom_agent.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Env;
class Window;
}

namespace ui_devtools {

class DOMAgentAura : public DOMAgent,
                     public aura::EnvObserver,
                     public aura::WindowObserver {
 public:
  explicit DOMAgentAura(aura::Env* env);
  ~DOMAgentAura() override;

  static DOMAgentAura* GetInstance() { return dom_agent_aura_; }

  void RegisterEnv(aura::Env* env);
  void RegisterRootWindow(aura::Window* root);
  const std::vector<aura::Window*>& root_windows() const { return roots_; }

 private:
  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  std::unique_ptr<protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root,
      aura::Window* window);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForRootWidget(
      UIElement* widget_element,
      views::Widget* widget);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForView(UIElement* view_element,
                                                        views::View* view);

  std::vector<UIElement*> CreateChildrenForRoot() override;
  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element) override;

  static DOMAgentAura* dom_agent_aura_;

  std::vector<aura::Env*> envs_;
  std::vector<aura::Window*> roots_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgentAura);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_AURA_H_
