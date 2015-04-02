// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/basic_focus_rules.h"

#include "base/macros.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"

using mojo::View;

namespace window_manager {

BasicFocusRules::BasicFocusRules(View* window_container)
    : window_container_(window_container) {
}

BasicFocusRules::~BasicFocusRules() {}

bool BasicFocusRules::SupportsChildActivation(View* view) const {
  return true;
}

bool BasicFocusRules::IsToplevelView(View* view) const {
  if (!IsViewParentedToWindowContainer(view))
    return false;

  // The window must exist within a container that supports activation.
  // The window cannot be blocked by a modal transient.
  return SupportsChildActivation(view->parent());
}

bool BasicFocusRules::CanActivateView(View* view) const {
  if (!view)
    return true;

  // Only toplevel windows can be activated
  if (!IsToplevelView(view))
    return false;

  // The view must be visible.
  if (!view->visible())
    return false;

  // TODO(erg): The aura version of this class asks the aura::Window's
  // ActivationDelegate whether the window is activatable.

  // A window must be focusable to be activatable. We don't call
  // CanFocusWindow() from here because it will call back to us via
  // GetActivatableWindow().
  if (!CanFocusViewImpl(view))
    return false;

  // TODO(erg): In the aura version, we also check whether the window is
  // blocked by a modal transient window.

  return true;
}

bool BasicFocusRules::CanFocusView(View* view) const {
  // It is possible to focus a NULL window, it is equivalent to clearing focus.
  if (!view)
    return true;

  // The focused view is always inside the active view, so views that aren't
  // activatable can't contain the focused view.
  View* activatable = GetActivatableView(view);
  if (!activatable || !activatable->Contains(view))
    return false;
  return CanFocusViewImpl(view);
}

View* BasicFocusRules::GetToplevelView(View* view) const {
  View* parent = view->parent();
  View* child = view;
  while (parent) {
    if (IsToplevelView(child))
      return child;

    parent = parent->parent();
    child = child->parent();
  }

  return nullptr;
}

View* BasicFocusRules::GetActivatableView(View* view) const {
  View* parent = view->parent();
  View* child = view;
  while (parent) {
    if (CanActivateView(child))
      return child;

    // TODO(erg): In the aura version of this class, we have a whole bunch of
    // checks to support modal transient windows, and transient parents.

    parent = parent->parent();
    child = child->parent();
  }

  return nullptr;
}

View* BasicFocusRules::GetFocusableView(View* view) const {
  if (CanFocusView(view))
    return view;

  // |view| may be in a hierarchy that is non-activatable, in which case we
  // need to cut over to the activatable hierarchy.
  View* activatable = GetActivatableView(view);
  if (!activatable) {
    // There may not be a related activatable hierarchy to cut over to, in which
    // case we try an unrelated one.
    View* toplevel = GetToplevelView(view);
    if (toplevel)
      activatable = GetNextActivatableView(toplevel);
    if (!activatable)
      return nullptr;
  }

  if (!activatable->Contains(view)) {
    // If there's already a child window focused in the activatable hierarchy,
    // just use that (i.e. don't shift focus), otherwise we need to at least cut
    // over to the activatable hierarchy.
    View* focused = GetFocusableView(activatable);
    return activatable->Contains(focused) ? focused : activatable;
  }

  while (view && !CanFocusView(view))
    view = view->parent();
  return view;
}

View* BasicFocusRules::GetNextActivatableView(View* activatable) const {
  DCHECK(activatable);

  // In the basic scenarios handled by BasicFocusRules, the pool of activatable
  // windows is limited to the |ignore|'s siblings.
  const View::Children& siblings = activatable->parent()->children();
  DCHECK(!siblings.empty());

  for (auto rit = siblings.rbegin(); rit != siblings.rend(); ++rit) {
    View* cur = *rit;
    if (cur == activatable)
      continue;
    if (CanActivateView(cur))
      return cur;
  }
  return nullptr;
}

// TODO(erg): aura::Window::CanFocus() exists. View::CanFocus() does
// not. This is a hack that does everything that Window::CanFocus() currently
// does that doesn't require a delegate or an EventClient.
bool BasicFocusRules::CanFocusViewImpl(View* view) const {
  // TODO(erg): In unit tests, views will never be drawn, so we can't rely on
  // IsDrawn() here.
  if (IsViewParentedToWindowContainer(view))
    return view->visible();

  // TODO(erg): Add the intermediary delegate and event client checks once we
  // have those.

  return CanFocusViewImpl(view->parent());
}

bool BasicFocusRules::IsViewParentedToWindowContainer(View* view) const {
  return view->parent() == window_container_;
}

}  // namespace mojo
