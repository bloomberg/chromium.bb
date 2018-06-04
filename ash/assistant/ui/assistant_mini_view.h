// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantMiniView : public views::View {
 public:
  AssistantMiniView();
  ~AssistantMiniView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  void InitLayout();

  DISALLOW_COPY_AND_ASSIGN(AssistantMiniView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
