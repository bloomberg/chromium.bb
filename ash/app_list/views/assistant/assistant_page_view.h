// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"

namespace ash {
class AssistantViewDelegate;
}  // namespace ash

namespace ui {
class LayerOwner;
}  // namespace ui

namespace app_list {

class AssistantMainView;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage {
 public:
  explicit AssistantPageView(
      ash::AssistantViewDelegate* assistant_view_delegate);
  ~AssistantPageView() override;

  void InitLayout();
  void Back();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListPage:
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;
  gfx::Rect GetSearchBoxBounds() const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

 private:
  ash::AssistantViewDelegate* const assistant_view_delegate_;

  // Owned by the views hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;

  // Used to enforce round corners on the Assistant view hierarchy.
  std::unique_ptr<ui::LayerOwner> mask_;

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
