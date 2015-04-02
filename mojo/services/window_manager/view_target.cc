// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/view_target.h"

#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/window_manager/view_targeter.h"
#include "mojo/services/window_manager/window_manager_app.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_property.h"
#include "ui/events/event.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace window_manager {

namespace {

DEFINE_OWNED_VIEW_PROPERTY_KEY(ViewTarget, kViewTargetKey, nullptr);

// Provides a version which keeps a copy of the data (for when it has to be
// derived instead of pointed at).
template <typename T>
class CopyingEventTargetIteratorImpl : public ui::EventTargetIterator {
 public:
  explicit CopyingEventTargetIteratorImpl(const std::vector<T*>& children)
      : children_(children),
        begin_(children_.rbegin()),
        end_(children_.rend()) {}
  ~CopyingEventTargetIteratorImpl() override {}

  ui::EventTarget* GetNextTarget() override {
    if (begin_ == end_)
      return nullptr;
    ui::EventTarget* target = *(begin_);
    ++begin_;
    return target;
  }

 private:
  typename std::vector<T*> children_;
  typename std::vector<T*>::const_reverse_iterator begin_;
  typename std::vector<T*>::const_reverse_iterator end_;
};

}  // namespace

ViewTarget::~ViewTarget() {
}

// static
ViewTarget* ViewTarget::TargetFromView(mojo::View* view) {
  if (!view)
    return nullptr;

  ViewTarget* target = view->GetLocalProperty(kViewTargetKey);
  if (target)
    return target;

  return new ViewTarget(view);
}

void ViewTarget::ConvertPointToTarget(const ViewTarget* source,
                                      const ViewTarget* target,
                                      gfx::Point* point) {
  // TODO(erg): Do we need to deal with |source| and |target| being in
  // different trees?
  DCHECK_EQ(source->GetRoot(), target->GetRoot());
  if (source == target)
    return;

  const ViewTarget* root_target = source->GetRoot();
  CHECK_EQ(root_target, target->GetRoot());

  if (source != root_target)
    source->ConvertPointForAncestor(root_target, point);
  if (target != root_target)
    target->ConvertPointFromAncestor(root_target, point);
}

std::vector<ViewTarget*> ViewTarget::GetChildren() const {
  std::vector<ViewTarget*> targets;
  for (mojo::View* child : view_->children())
    targets.push_back(TargetFromView(child));
  return targets;
}

const ViewTarget* ViewTarget::GetParent() const {
  return TargetFromView(view_->parent());
}

gfx::Rect ViewTarget::GetBounds() const {
  return view_->bounds().To<gfx::Rect>();
}

bool ViewTarget::HasParent() const {
  return !!view_->parent();
}

bool ViewTarget::IsVisible() const {
  return view_->visible();
}

const ViewTarget* ViewTarget::GetRoot() const {
  const ViewTarget* root = this;
  for (const ViewTarget* parent = this; parent; parent = parent->GetParent())
    root = parent;
  return root;
}

scoped_ptr<ViewTargeter> ViewTarget::SetEventTargeter(
    scoped_ptr<ViewTargeter> targeter) {
  scoped_ptr<ViewTargeter> old_targeter = targeter_.Pass();
  targeter_ = targeter.Pass();
  return old_targeter.Pass();
}

bool ViewTarget::CanAcceptEvent(const ui::Event& event) {
  // We need to make sure that a touch cancel event and any gesture events it
  // creates can always reach the window. This ensures that we receive a valid
  // touch / gesture stream.
  if (event.IsEndingEvent())
    return true;

  if (!view_->visible())
    return false;

  // The top-most window can always process an event.
  if (!view_->parent())
    return true;

  // In aura, we only accept events if this is a key event or if the user
  // supplied a TargetHandler, usually the aura::WindowDelegate. Here, we're
  // just forwarding events to other Views which may be in other processes, so
  // always accept.
  return true;
}

ui::EventTarget* ViewTarget::GetParentTarget() {
  return TargetFromView(view_->parent());
}

scoped_ptr<ui::EventTargetIterator> ViewTarget::GetChildIterator() const {
  return scoped_ptr<ui::EventTargetIterator>(
      new CopyingEventTargetIteratorImpl<ViewTarget>(GetChildren()));
}

ui::EventTargeter* ViewTarget::GetEventTargeter() {
  return targeter_.get();
}

void ViewTarget::ConvertEventToTarget(ui::EventTarget* target,
                                      ui::LocatedEvent* event) {
  event->ConvertLocationToTarget(this, static_cast<ViewTarget*>(target));
}

ViewTarget::ViewTarget(mojo::View* view_to_wrap) : view_(view_to_wrap) {
  DCHECK(view_->GetLocalProperty(kViewTargetKey) == nullptr);
  view_->SetLocalProperty(kViewTargetKey, this);
}

bool ViewTarget::ConvertPointForAncestor(const ViewTarget* ancestor,
                                         gfx::Point* point) const {
  gfx::Vector2d offset;
  bool result = GetTargetOffsetRelativeTo(ancestor, &offset);
  *point += offset;
  return result;
}

bool ViewTarget::ConvertPointFromAncestor(const ViewTarget* ancestor,
                                          gfx::Point* point) const {
  gfx::Vector2d offset;
  bool result = GetTargetOffsetRelativeTo(ancestor, &offset);
  *point -= offset;
  return result;
}

bool ViewTarget::GetTargetOffsetRelativeTo(const ViewTarget* ancestor,
                                           gfx::Vector2d* offset) const {
  const ViewTarget* v = this;
  for (; v && v != ancestor; v = v->GetParent()) {
    gfx::Rect bounds = v->GetBounds();
    *offset += gfx::Vector2d(bounds.x(), bounds.y());
  }
  return v == ancestor;
}

}  // namespace window_manager
