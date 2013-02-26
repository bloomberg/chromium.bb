// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

namespace content {
class WebContents;
}
namespace views {
namespace internal {
class NativeWidgetDelegate;
}
class NativeWidget;
class NonClientFrameView;
class WidgetDelegate;
}

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowViews
//
//  A web contents modal dialog that implements the dialog as a child HWND with
//  a custom window frame. The ConstrainedWindowViews owns itself and will be
//  deleted soon after being closed.
//
class ConstrainedWindowViews : public views::Widget {
 public:
  ConstrainedWindowViews(gfx::NativeView parent,
                         bool off_the_record,
                         views::WidgetDelegate* widget_delegate);
  virtual ~ConstrainedWindowViews();

  // TODO(wittman): Remove in favor of native equivalents.
  void ShowWebContentsModalDialog();
  void CloseWebContentsModalDialog();
  void FocusWebContentsModalDialog();
  void PulseWebContentsModalDialog();
  NativeWebContentsModalDialog GetNativeDialog();

  // Factory function for the class (temporary).
  static ConstrainedWindowViews* Create(content::WebContents* web_contents,
                                        views::WidgetDelegate* widget_delegate);

 private:
  // Overridden from views::Widget:
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  bool off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
