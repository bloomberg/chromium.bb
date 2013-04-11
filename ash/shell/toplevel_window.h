// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_TOPLEVEL_WINDOW_H_
#define ASH_SHELL_TOPLEVEL_WINDOW_H_

#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace shell {

class ToplevelWindow : public views::WidgetDelegateView {
 public:
  struct CreateParams {
    CreateParams();

    bool can_resize;
    bool can_maximize;
    bool persist_across_all_workspaces;
  };
  static void CreateToplevelWindow(const CreateParams& params);

 private:
  explicit ToplevelWindow(const CreateParams& params);
  virtual ~ToplevelWindow();

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;

  const CreateParams params_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindow);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_TOPLEVEL_WINDOW_H_
