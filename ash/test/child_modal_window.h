// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_CHILD_WMODAL_WINDOW_H_
#define ASH_TEST_CHILD_WMODAL_WINDOW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class NativeTextButton;
class NativeViewHost;
class Textfield;
class View;
class Widget;
}

namespace ash {
namespace test {

void CreateChildModalParent();

class ChildModalParent : public views::WidgetDelegateView,
                         public views::ButtonListener,
                         public views::WidgetObserver {
 public:
  ChildModalParent();
  virtual ~ChildModalParent();

  void ShowChild();
  gfx::NativeWindow GetModalParent() const;
  gfx::NativeWindow GetChild() const;

 private:
  views::Widget* CreateChild();

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // The button to toggle showing and hiding the child window. The child window
  // does not block input to this button.
  views::NativeTextButton* button_;

  // The text field to indicate the keyboard focus.
  views::Textfield* textfield_;

  // The host for the modal parent.
  views::NativeViewHost* host_;

  // The modal parent of the child window. The child window blocks input to this
  // view.
  gfx::NativeWindow modal_parent_;

  // The child window.
  views::Widget* child_;

  DISALLOW_COPY_AND_ASSIGN(ChildModalParent);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_CHILD_WMODAL_WINDOW_H_
