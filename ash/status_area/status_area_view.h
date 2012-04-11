// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STATUS_AREA_STATUS_AREA_VIEW_H_
#define ASH_STATUS_AREA_STATUS_AREA_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

class FocusCycler;

class ASH_EXPORT StatusAreaView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView {
 public:
  StatusAreaView();
  virtual ~StatusAreaView();

  // Sets the focus cycler.
  void SetFocusCyclerForTesting(const FocusCycler* focus_cycler);

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // views::WidgetDelegate overrides:
  virtual bool CanActivate() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  const FocusCycler* focus_cycler_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_STATUS_AREA_STATUS_AREA_VIEW_H_
