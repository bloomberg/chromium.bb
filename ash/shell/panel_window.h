// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PANEL_WINDOW_H_
#define ASH_SHELL_PANEL_WINDOW_H_

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class PanelFrameView;

// Example Class for panel windows (Widget::InitParams::TYPE_PANEL).
// Instances of PanelWindow will get added to the PanelContainer top level
// window which manages the panel layout through PanelLayoutManager.
class PanelWindow : public views::WidgetDelegateView {
 public:
  explicit PanelWindow(const std::string& name);
  virtual ~PanelWindow();

  // Creates the widget for the panel window using |params_|.
  views::Widget* CreateWidget();

  const std::string& name() { return name_; }
  views::Widget::InitParams& params() { return params_; }

  // Creates a panel window and returns the associated widget.
  static views::Widget* CreatePanelWindow(const gfx::Rect& rect);

 private:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;

  std::string name_;
  views::Widget::InitParams params_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindow);
};

}  // namespace ash

#endif  // ASH_SHELL_PANEL_WINDOW_H_
