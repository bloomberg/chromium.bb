// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
#define ASH_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace aura {
class Window;
}

namespace ash {

namespace devtools {

class ASH_EXPORT AshDevToolsDOMAgentObserver {
 public:
  virtual void OnWindowBoundsChanged(aura::Window* window) {}
  virtual void OnWidgetBoundsChanged(views::Widget* widget) {}
  virtual void OnViewBoundsChanged(views::View* view) {}
};

class ASH_EXPORT AshDevToolsDOMAgent
    : public NON_EXPORTED_BASE(ui::devtools::UiDevToolsBaseAgent<
                               ui::devtools::protocol::DOM::Metainfo>),
      public aura::WindowObserver,
      public views::WidgetObserver,
      public views::WidgetRemovalsObserver,
      public views::ViewObserver {
 public:
  AshDevToolsDOMAgent();
  ~AshDevToolsDOMAgent() override;

  // DOM::Backend
  ui::devtools::protocol::Response disable() override;
  ui::devtools::protocol::Response getDocument(
      std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) override;
  ui::devtools::protocol::Response highlightNode(
      std::unique_ptr<ui::devtools::protocol::DOM::HighlightConfig>
          highlight_config,
      ui::devtools::protocol::Maybe<int> node_id) override;
  ui::devtools::protocol::Response hideHighlight() override;

  // WindowObserver
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // views::WidgetRemovalsObserver
  void OnWillRemoveView(views::Widget* widget, views::View* view) override;

  // views::WidgetObserver
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::ViewObserver
  void OnChildViewRemoved(views::View* parent, views::View* view) override;
  void OnChildViewAdded(views::View* parent, views::View* view) override;
  void OnChildViewReordered(views::View* parent, views::View*) override;
  void OnViewBoundsChanged(views::View* view) override;

  aura::Window* GetWindowFromNodeId(int nodeId);
  views::Widget* GetWidgetFromNodeId(int nodeId);
  views::View* GetViewFromNodeId(int nodeId);

  int GetNodeIdFromWindow(aura::Window* window);
  int GetNodeIdFromWidget(views::Widget* widget);
  int GetNodeIdFromView(views::View* view);

  void AddObserver(AshDevToolsDOMAgentObserver* observer);
  void RemoveObserver(AshDevToolsDOMAgentObserver* observer);

 private:
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildInitialTree();
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForWindow(
      aura::Window* window);
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForRootWidget(
      views::Widget* widget);
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForView(
      views::View* view);

  void AddWindowTree(aura::Window* window);
  // |remove_observer| ensures that we don't add a duplicate observer in any
  // observer callback. For example, |remove_observer| is false when rebuilding
  // the tree in OnWindowStackingChanged so that the observer is not removed and
  // re-added, thus causing endless calls on the observer.
  void RemoveWindowTree(aura::Window* window, bool remove_observer);
  void RemoveWindowNode(aura::Window* window, bool remove_observer);

  // Don't need AddWidgetTree because |widget| will always be inside a window,
  // so when windows are created, their widget nodes are created as well.
  void RemoveWidgetTree(views::Widget* widget, bool remove_observer);
  void RemoveWidgetNode(views::Widget* widget, bool remove_observer);

  void AddViewTree(views::View* view);
  void RemoveViewTree(views::View* view,
                      views::View* parent,
                      bool remove_observer);
  void RemoveViewNode(views::View* view,
                      views::View* parent,
                      bool remove_observer);

  void DestroyHighlightingWidget();
  void RemoveObservers();
  void Reset();

  using WindowAndBoundsPair = std::pair<aura::Window*, gfx::Rect>;
  WindowAndBoundsPair GetNodeWindowAndBounds(int node_id);
  void InitializeHighlightingWidget();
  void UpdateHighlight(const WindowAndBoundsPair& window_and_bounds,
                       SkColor background,
                       SkColor border);
  ui::devtools::protocol::Response HighlightNode(
      std::unique_ptr<ui::devtools::protocol::DOM::HighlightConfig>
          highlight_config,
      int node_id);
  bool IsHighlightingWindow(aura::Window* window);

  std::unique_ptr<views::Widget> widget_for_highlighting_;

  using WindowToNodeIdMap = std::unordered_map<aura::Window*, int>;
  WindowToNodeIdMap window_to_node_id_map_;
  using NodeIdToWindowMap = std::unordered_map<int, aura::Window*>;
  NodeIdToWindowMap node_id_to_window_map_;

  using WidgetToNodeIdMap = std::unordered_map<views::Widget*, int>;
  WidgetToNodeIdMap widget_to_node_id_map_;
  using NodeIdToWidgetMap = std::unordered_map<int, views::Widget*>;
  NodeIdToWidgetMap node_id_to_widget_map_;

  using ViewToNodeIdMap = std::unordered_map<views::View*, int>;
  ViewToNodeIdMap view_to_node_id_map_;
  using NodeIdToViewMap = std::unordered_map<int, views::View*>;
  NodeIdToViewMap node_id_to_view_map_;

  base::ObserverList<AshDevToolsDOMAgentObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsDOMAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
