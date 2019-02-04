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

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage {
 public:
  explicit AssistantPageView(
      ash::AssistantViewDelegate* assistant_view_delegate);
  ~AssistantPageView() override;

  void InitLayout();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;

  // AppListPage:
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

 private:
  // Owned by the views hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
