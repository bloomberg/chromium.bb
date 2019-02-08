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

namespace app_list {

class AssistantMainView;
class ContentsView;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage {
 public:
  AssistantPageView(ContentsView* contents_view,
                    ash::AssistantViewDelegate* assistant_view_delegate);
  ~AssistantPageView() override;

  void InitLayout();
  void Back();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListPage:
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;
  gfx::Rect GetSearchBoxBounds() const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

 private:
  // Owned by the views hierarchy.
  ContentsView* contents_view_ = nullptr;
  AssistantMainView* assistant_main_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
