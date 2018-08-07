// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class StyledLabel;
}  // namespace views

namespace ash {

class AssistantOptInView : public views::View {
 public:
  AssistantOptInView();
  ~AssistantOptInView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void InitLayout();

  views::StyledLabel* label_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
