// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
#define ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_

#include "ash/common/wm_shell.h"
#include "ash/common/wm_window_observer.h"
#include "base/compiler_specific.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace devtools {

class ASH_EXPORT AshDevToolsDOMAgent
    : public NON_EXPORTED_BASE(ui::devtools::UiDevToolsBaseAgent<
                               ui::devtools::protocol::DOM::Metainfo>),
      public WmWindowObserver {
 public:
  explicit AshDevToolsDOMAgent(ash::WmShell* shell);
  ~AshDevToolsDOMAgent() override;

  // DOM::Backend
  ui::devtools::protocol::Response disable() override;
  ui::devtools::protocol::Response getDocument(
      std::unique_ptr<ui::devtools::protocol::DOM::Node>* out_root) override;

  // WindowObserver
  void OnWindowTreeChanging(WmWindow* window,
                            const TreeChangeParams& params) override;
  void OnWindowTreeChanged(WmWindow* window,
                           const TreeChangeParams& params) override;
  void OnWindowStackingChanged(WmWindow* window) override;

  WmWindow* GetWindowFromNodeId(int nodeId);
  views::Widget* GetWidgetFromNodeId(int nodeId);
  views::View* GetViewFromNodeId(int nodeId);

  int GetNodeIdFromWindow(WmWindow* window);
  int GetNodeIdFromWidget(views::Widget* widget);
  int GetNodeIdFromView(views::View* view);

 private:
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildInitialTree();
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForWindow(
      WmWindow* window);
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForRootWidget(
      views::Widget* widget);
  std::unique_ptr<ui::devtools::protocol::DOM::Node> BuildTreeForView(
      views::View* view);

  void AddWindowTree(WmWindow* window);
  // |remove_observer| ensures that we don't add a duplicate observer in any
  // observer callback. For example, |remove_observer| is false when rebuilding
  // the tree in OnWindowStackingChanged so that the observer is not removed and
  // re-added, thus causing endless calls on the observer.
  void RemoveWindowTree(WmWindow* window, bool remove_observer);
  void RemoveWindowNode(WmWindow* window, bool remove_observer);

  void RemoveWidgetTree(views::Widget* widget, bool remove_observer);
  void RemoveWidgetNode(views::Widget* widget, bool remove_observer);

  void RemoveViewTree(views::View* view,
                      views::View* parent,
                      bool remove_observer);
  void RemoveViewNode(views::View* view,
                      views::View* parent,
                      bool remove_observer);

  void RemoveObserverFromAllWindows();
  void Reset();

  ash::WmShell* shell_;

  using WindowToNodeIdMap = std::unordered_map<WmWindow*, int>;
  WindowToNodeIdMap window_to_node_id_map_;
  using NodeIdToWindowMap = std::unordered_map<int, WmWindow*>;
  NodeIdToWindowMap node_id_to_window_map_;

  using WidgetToNodeIdMap = std::unordered_map<views::Widget*, int>;
  WidgetToNodeIdMap widget_to_node_id_map_;
  using NodeIdToWidgetMap = std::unordered_map<int, views::Widget*>;
  NodeIdToWidgetMap node_id_to_widget_map_;

  using ViewToNodeIdMap = std::unordered_map<views::View*, int>;
  ViewToNodeIdMap view_to_node_id_map_;
  using NodeIdToViewMap = std::unordered_map<int, views::View*>;
  NodeIdToViewMap node_id_to_view_map_;

  DISALLOW_COPY_AND_ASSIGN(AshDevToolsDOMAgent);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_COMMON_DEVTOOLS_ASH_DEVTOOLS_DOM_AGENT_H_
