// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_VIEW_TARGET_H_
#define SERVICES_WINDOW_MANAGER_VIEW_TARGET_H_

#include "ui/events/event_target.h"

namespace gfx {
class Point;
class Rect;
class Vector2d;
}

namespace ui {
class EventTargeter;
}

namespace mojo {
class View;
}

namespace window_manager {

class TestView;
class ViewTargeter;
class WindowManagerApp;

// A wrapper class around mojo::View; we can't subclass View to implement the
// event targeting interfaces, so we create a separate object which observes
// the View and ties its lifetime to it.
//
// We set ourselves as a property of the view passed in, and we are owned by
// said View.
class ViewTarget : public ui::EventTarget {
 public:
  ~ViewTarget() override;

  // Returns the ViewTarget for a View. ViewTargets are owned by the |view|
  // passed in, and are created on demand.
  static ViewTarget* TargetFromView(mojo::View* view);

  // Converts |point| from |source|'s coordinates to |target|'s. If |source| is
  // NULL, the function returns without modifying |point|. |target| cannot be
  // NULL.
  static void ConvertPointToTarget(const ViewTarget* source,
                                   const ViewTarget* target,
                                   gfx::Point* point);

  mojo::View* view() { return view_; }

  // TODO(erg): Make this const once we've removed aura from the tree and it's
  // feasible to change all callers of the EventTargeter interface to pass and
  // accept const objects. (When that gets done, re-const the
  // EventTargetIterator::GetNextTarget and EventTarget::GetChildIterator
  // interfaces.)
  std::vector<ViewTarget*> GetChildren() const;

  const ViewTarget* GetParent() const;
  gfx::Rect GetBounds() const;
  bool HasParent() const;
  bool IsVisible() const;

  const ViewTarget* GetRoot() const;

  // Sets a new ViewTargeter for the view, and returns the previous
  // ViewTargeter.
  scoped_ptr<ViewTargeter> SetEventTargeter(scoped_ptr<ViewTargeter> targeter);

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  scoped_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;
  void ConvertEventToTarget(ui::EventTarget* target,
                            ui::LocatedEvent* event) override;

 private:
  friend class TestView;
  explicit ViewTarget(mojo::View* view_to_wrap);

  bool ConvertPointForAncestor(const ViewTarget* ancestor,
                               gfx::Point* point) const;
  bool ConvertPointFromAncestor(const ViewTarget* ancestor,
                                gfx::Point* point) const;
  bool GetTargetOffsetRelativeTo(const ViewTarget* ancestor,
                                 gfx::Vector2d* offset) const;

  // The mojo::View that we dispatch to.
  mojo::View* view_;

  scoped_ptr<ViewTargeter> targeter_;

  DISALLOW_COPY_AND_ASSIGN(ViewTarget);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_VIEW_TARGET_H_
