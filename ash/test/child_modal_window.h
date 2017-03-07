// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_CHILD_MODAL_WINDOW_H_
#define ASH_TEST_CHILD_MODAL_WINDOW_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class LabelButton;
class NativeViewHost;
class Textfield;
class View;
class Widget;
}

namespace ash {
namespace test {

void CreateChildModalParent(aura::Window* context);

class ChildModalParent : public views::WidgetDelegateView,
                         public views::ButtonListener,
                         public views::WidgetObserver {
 public:
  ChildModalParent(aura::Window* context);
  ~ChildModalParent() override;

  void ShowChild();
  aura::Window* GetModalParent() const;
  aura::Window* GetChild() const;

 private:
  views::Widget* CreateChild();

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool CanResize() const override;
  void DeleteDelegate() override;

  // Overridden from views::View:
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Overridden from ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // This is the Widget this class creates to contain the child Views. This
  // class is *not* the WidgetDelegateView for this Widget.
  std::unique_ptr<views::Widget> widget_;

  // The button to toggle showing and hiding the child window. The child window
  // does not block input to this button.
  views::LabelButton* button_;

  // The text field to indicate the keyboard focus.
  views::Textfield* textfield_;

  // The host for the modal parent.
  views::NativeViewHost* host_;

  // The child window.
  views::Widget* child_;

  DISALLOW_COPY_AND_ASSIGN(ChildModalParent);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_CHILD_MODAL_WINDOW_H_
