// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class TabbedPane;
class Widget;
}  // namespace views

namespace keyboard_shortcut_viewer {

class KeyboardShortcutItemView;
class KSVSearchBoxView;
enum class ShortcutCategory;

// The UI container for Ash and Chrome keyboard shortcuts.
class KeyboardShortcutView : public views::WidgetDelegateView,
                             public search_box::SearchBoxViewDelegate {
 public:
  ~KeyboardShortcutView() override;

  // Shows the Keyboard Shortcut Viewer window, or re-activates an existing one.
  static views::Widget* Show(gfx::NativeWindow context);

  // views::View:
  void Layout() override;

  // search_box::SearchBoxViewDelegate:
  void QueryChanged(search_box::SearchBoxViewBase* sender) override;
  void BackButtonPressed() override;
  void ActiveChanged(search_box::SearchBoxViewBase* sender) override;

 private:
  friend class KeyboardShortcutViewTest;

  KeyboardShortcutView();

  void InitViews();

  // Put focus on the active tab. Used when the first time to show the widget or
  // after exiting search mode.
  void RequestFocusForActiveTab();

  // Update views' layout based on search box status.
  void UpdateViewsLayout(bool is_search_box_active);

  static KeyboardShortcutView* GetInstanceForTesting();
  int GetTabCountForTesting() const;
  const std::vector<KeyboardShortcutItemView*>& GetShortcutViewsForTesting() {
    return shortcut_views_;
  }

  // views::WidgetDelegate:
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // Owned by views hierarchy.
  views::TabbedPane* tabbed_pane_;
  views::View* search_results_container_;

  // SearchBoxViewBase is a WidgetDelegateView, which owns itself and cannot be
  // deleted from the views hierarchy automatically.
  std::unique_ptr<KSVSearchBoxView> search_box_view_;

  // Contains all the shortcut item views from all categories. This list is used
  // for searching. The views are owned by the Views hierarchy.
  std::vector<KeyboardShortcutItemView*> shortcut_views_;

  // An illustration to indicate no search results found. Since this view need
  // to be added and removed frequently from the |search_results_container_|, it
  // is not owned by view hierarchy to avoid recreating it.
  std::unique_ptr<views::View> search_no_result_view_;

  // Cached value of search box text status. When the status changes, need to
  // update views' layout.
  bool is_search_box_empty_ = true;

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
