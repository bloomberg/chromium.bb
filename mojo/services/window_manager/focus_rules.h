// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_FOCUS_RULES_H_
#define SERVICES_WINDOW_MANAGER_FOCUS_RULES_H_

#include "third_party/mojo_services/src/view_manager/public/cpp/types.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"

namespace window_manager {

// Implemented by an object that establishes the rules about what can be
// focused or activated.
class FocusRules {
 public:
  virtual ~FocusRules() {}

  // Returns true if the children of |window| can be activated.
  virtual bool SupportsChildActivation(mojo::View* window) const = 0;

  // Returns true if |view| is a toplevel view. Whether or not a view
  // is considered toplevel is determined by a similar set of rules that
  // govern activation and focus. Not all toplevel views are activatable,
  // call CanActivateView() to determine if a view can be activated.
  virtual bool IsToplevelView(mojo::View* view) const = 0;
  // Returns true if |view| can be activated or focused.
  virtual bool CanActivateView(mojo::View* view) const = 0;
  // For CanFocusView(), null is supported, because null is a valid focusable
  // view (in the case of clearing focus).
  virtual bool CanFocusView(mojo::View* view) const = 0;

  // Returns the toplevel view containing |view|. Not all toplevel views
  // are activatable, call GetActivatableView() instead to return the
  // activatable view, which might be in a different hierarchy.
  // Will return null if |view| is not contained by a view considered to be
  // a toplevel view.
  virtual mojo::View* GetToplevelView(mojo::View* view) const = 0;
  // Returns the activatable or focusable view given an attempt to activate or
  // focus |view|. Some possible scenarios (not intended to be exhaustive):
  // - |view| is a child of a non-focusable view and so focus must be set
  //   according to rules defined by the delegate, e.g. to a parent.
  // - |view| is an activatable view that is the transient parent of a modal
  //   view, so attempts to activate |view| should result in the modal
  //   transient being activated instead.
  // These methods may return null if they are unable to find an activatable
  // or focusable view given |view|.
  virtual mojo::View* GetActivatableView(mojo::View* view) const = 0;
  virtual mojo::View* GetFocusableView(mojo::View* view) const = 0;

  // Returns the next view to activate in the event that |ignore| is no longer
  // activatable. This function is called when something is happening to
  // |ignore| that means it can no longer have focus or activation, including
  // but not limited to:
  // - it or its parent hierarchy is being hidden, or removed from the
  //   RootView.
  // - it is being destroyed.
  // - it is being explicitly deactivated.
  // |ignore| cannot be null.
  virtual mojo::View* GetNextActivatableView(mojo::View* ignore) const = 0;
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_FOCUS_RULES_H_
