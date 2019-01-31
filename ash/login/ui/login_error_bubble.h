// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT LoginErrorBubble : public LoginBaseBubbleView {
 public:
  LoginErrorBubble();
  LoginErrorBubble(views::View* content,
                   views::View* anchor_view,
                   bool is_persistent);
  ~LoginErrorBubble() override;

  void SetContent(views::View* content);

  // LoginBaseBubbleView:
  bool IsPersistent() const override;
  void SetPersistent(bool persistent) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  views::View* content_ = nullptr;
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(LoginErrorBubble);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
