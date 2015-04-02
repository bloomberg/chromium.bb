// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_BASIC_FOCUS_RULES_H_
#define SERVICES_WINDOW_MANAGER_BASIC_FOCUS_RULES_H_

#include "mojo/services/window_manager/focus_rules.h"

namespace mojo {
class View;
}

namespace window_manager {

// The focusing rules used inside a window manager.
//
// This is intended to be a user supplyable, subclassable component passed to
// WindowManagerApp, allowing for the creation of other window managers.
class BasicFocusRules : public FocusRules {
 public:
  BasicFocusRules(mojo::View* window_container);
  ~BasicFocusRules() override;

 protected:
  // Overridden from mojo::FocusRules:
  bool SupportsChildActivation(mojo::View* view) const override;
  bool IsToplevelView(mojo::View* view) const override;
  bool CanActivateView(mojo::View* view) const override;
  bool CanFocusView(mojo::View* view) const override;
  mojo::View* GetToplevelView(mojo::View* view) const override;
  mojo::View* GetActivatableView(mojo::View* view) const override;
  mojo::View* GetFocusableView(mojo::View* view) const override;
  mojo::View* GetNextActivatableView(mojo::View* activatable) const override;

 private:
  bool CanFocusViewImpl(mojo::View* view) const;

  // Tests to see if |view| is in |window_container_|.
  bool IsViewParentedToWindowContainer(mojo::View* view) const;

  mojo::View* window_container_;

  DISALLOW_COPY_AND_ASSIGN(BasicFocusRules);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_BASIC_FOCUS_RULES_H_
