// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel_stack.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class StackedPanelCollection;

// A native window that acts as the owner of all panels in the stack, in order
// to make all panels appear as a single window on the taskbar or launcher.
class PanelStackView : public NativePanelStack,
                       public views::WidgetObserver,
                       public views::WidgetDelegateView {
 public:
  explicit PanelStackView(
      scoped_ptr<StackedPanelCollection> stacked_collection);
  ~PanelStackView();

 protected:
  // Overridden from NativePanelStack:
  virtual void Close() OVERRIDE;
  virtual void OnPanelAddedOrRemoved(Panel* panel) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

 private:
  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  void EnsureInitialized();

  // Updates the owner of the underlying window such that multiple panels
  // stacked together could appear as a single window on the taskbar or
  // launcher.
  void UpdateWindowOwnerForTaskbarIconAppearance(Panel* panel);

  scoped_ptr<StackedPanelCollection> stacked_collection_;

  bool delay_initialized_;

  views::Widget* window_;

  DISALLOW_COPY_AND_ASSIGN(PanelStackView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
