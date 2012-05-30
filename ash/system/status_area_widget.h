// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class StatusAreaWidgetDelegate;

class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  StatusAreaWidget();
  virtual ~StatusAreaWidget();

  void AddTray(views::View* tray);
  void SetShelfAlignment(ShelfAlignment alignment);

 private:
  internal::StatusAreaWidgetDelegate* widget_delegate_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
