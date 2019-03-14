// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/view.h"

namespace ash {
class AssistantViewDelegate;
}  // namespace ash

namespace app_list {

class AssistantMainStage;
class DialogPlate;

class APP_LIST_EXPORT AssistantMainView : public views::View {
 public:
  explicit AssistantMainView(ash::AssistantViewDelegate* delegate);
  ~AssistantMainView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void RequestFocus() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();

  ash::AssistantViewDelegate* const delegate_;

  DialogPlate* dialog_plate_;            // Owned by view hierarchy.
  AssistantMainStage* main_stage_;       // Owned by view hierarchy.

  // Used to enforce child layers clip to rounded corner bounds.
  std::unique_ptr<ui::LayerOwner> layer_mask_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMainView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
