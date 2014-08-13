// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view.h"

#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/gfx/canvas.h"

namespace mojo {

namespace {

void NotifyViewTreeChangeAtReceiver(
    View* receiver,
    const ViewObserver::TreeChangeParams& params,
    bool change_applied) {
  ViewObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  if (change_applied) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(receiver).observers(),
                      OnTreeChanged(local_params));
  } else {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(receiver).observers(),
                      OnTreeChanging(local_params));
  }
}

void NotifyViewTreeChangeUp(
    View* start_at,
    const ViewObserver::TreeChangeParams& params,
    bool change_applied) {
  for (View* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params, change_applied);
}

void NotifyViewTreeChangeDown(
    View* start_at,
    const ViewObserver::TreeChangeParams& params,
    bool change_applied) {
  NotifyViewTreeChangeAtReceiver(start_at, params, change_applied);
  View::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params, change_applied);
}

void NotifyViewTreeChange(
    const ViewObserver::TreeChangeParams& params,
    bool change_applied) {
  NotifyViewTreeChangeDown(params.target, params, change_applied);
  if (params.old_parent)
    NotifyViewTreeChangeUp(params.old_parent, params, change_applied);
  if (params.new_parent)
    NotifyViewTreeChangeUp(params.new_parent, params, change_applied);
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(View* target, View* old_parent, View* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_, false);
  }
  ~ScopedTreeNotifier() {
    NotifyViewTreeChange(params_, true);
  }

 private:
  ViewObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(View* child, View::Children* children) {
  View::Children::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    ViewPrivate(child).ClearParent();
  }
}

class ScopedOrderChangedNotifier {
 public:
  ScopedOrderChangedNotifier(View* view,
                             View* relative_view,
                             OrderDirection direction)
      : view_(view),
        relative_view_(relative_view),
        direction_(direction) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view_).observers(),
                      OnViewReordering(view_, relative_view_, direction_));
  }
  ~ScopedOrderChangedNotifier() {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view_).observers(),
                      OnViewReordered(view_, relative_view_, direction_));
  }

 private:
  View* view_;
  View* relative_view_;
  OrderDirection direction_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOrderChangedNotifier);
};

// Returns true if the order actually changed.
bool ReorderImpl(View::Children* children,
                 View* view,
                 View* relative,
                 OrderDirection direction) {
  DCHECK(relative);
  DCHECK_NE(view, relative);
  DCHECK_EQ(view->parent(), relative->parent());

  const size_t child_i =
      std::find(children->begin(), children->end(), view) - children->begin();
  const size_t target_i =
      std::find(children->begin(), children->end(), relative) -
      children->begin();
  if ((direction == ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  ScopedOrderChangedNotifier notifier(view, relative, direction);

  const size_t dest_i = direction == ORDER_DIRECTION_ABOVE
                            ? (child_i < target_i ? target_i : target_i + 1)
                            : (child_i < target_i ? target_i - 1 : target_i);
  children->erase(children->begin() + child_i);
  children->insert(children->begin() + dest_i, view);

  return true;
}

class ScopedSetBoundsNotifier {
 public:
  ScopedSetBoundsNotifier(View* view,
                          const gfx::Rect& old_bounds,
                          const gfx::Rect& new_bounds)
      : view_(view),
        old_bounds_(old_bounds),
        new_bounds_(new_bounds) {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view_).observers(),
                      OnViewBoundsChanging(view_, old_bounds_, new_bounds_));
  }
  ~ScopedSetBoundsNotifier() {
    FOR_EACH_OBSERVER(ViewObserver,
                      *ViewPrivate(view_).observers(),
                      OnViewBoundsChanged(view_, old_bounds_, new_bounds_));
  }

 private:
  View* view_;
  const gfx::Rect old_bounds_;
  const gfx::Rect new_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetBoundsNotifier);
};

// Some operations are only permitted in the connection that created the view.
bool OwnsView(ViewManager* manager, View* view) {
  return !manager ||
      static_cast<ViewManagerClientImpl*>(manager)->OwnsView(view->id());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// View, public:

// static
View* View::Create(ViewManager* view_manager) {
  View* view = new View(view_manager);
  static_cast<ViewManagerClientImpl*>(view_manager)->AddView(view);
  return view;
}

void View::Destroy() {
  if (!OwnsView(manager_, this))
    return;

  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->DestroyView(id_);
  while (!children_.empty()) {
    View* child = children_.front();
    if (!OwnsView(manager_, child)) {
      ViewPrivate(child).ClearParent();
      children_.erase(children_.begin());
    } else {
      child->Destroy();
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    }
  }
  LocalDestroy();
}

void View::SetBounds(const gfx::Rect& bounds) {
  if (!OwnsView(manager_, this))
    return;

  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->SetBounds(id_, bounds);
  LocalSetBounds(bounds_, bounds);
}

void View::SetVisible(bool value) {
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->SetVisible(id_, value);
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::AddChild(View* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (manager_)
    CHECK_EQ(ViewPrivate(child).view_manager(), manager_);
  LocalAddChild(child);
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->AddChild(child->id(), id_);
}

void View::RemoveChild(View* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (manager_)
    CHECK_EQ(ViewPrivate(child).view_manager(), manager_);
  LocalRemoveChild(child);
  if (manager_) {
    static_cast<ViewManagerClientImpl*>(manager_)->RemoveChild(child->id(),
                                                               id_);
  }
}

void View::MoveToFront() {
  Reorder(parent_->children_.back(), ORDER_DIRECTION_ABOVE);
}

void View::MoveToBack() {
  Reorder(parent_->children_.front(), ORDER_DIRECTION_BELOW);
}

void View::Reorder(View* relative, OrderDirection direction) {
  if (!LocalReorder(relative, direction))
    return;
  if (manager_) {
    static_cast<ViewManagerClientImpl*>(manager_)->Reorder(id_,
                                                            relative->id(),
                                                            direction);
  }
}

bool View::Contains(View* child) const {
  if (manager_)
    CHECK_EQ(ViewPrivate(child).view_manager(), manager_);
  for (View* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

View* View::GetChildById(Id id) {
  if (id == id_)
    return this;
  // TODO(beng): this could be improved depending on how we decide to own views.
  Children::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    View* view = (*it)->GetChildById(id);
    if (view)
      return view;
  }
  return NULL;
}

void View::SetContents(const SkBitmap& contents) {
  if (manager_) {
    static_cast<ViewManagerClientImpl*>(manager_)->SetViewContents(id_,
        contents);
  }
}

void View::SetColor(SkColor color) {
  gfx::Canvas canvas(bounds_.size(), 1.0f, true);
  canvas.DrawColor(color);
  SetContents(skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(true));
}

void View::SetFocus() {
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->SetFocus(id_);
}

void View::Embed(const String& url) {
  static_cast<ViewManagerClientImpl*>(manager_)->Embed(url, id_);
}

scoped_ptr<ServiceProvider>
    View::Embed(const String& url,
                scoped_ptr<ServiceProviderImpl> exported_services) {
  scoped_ptr<ServiceProvider> imported_services;
  // BindToProxy() takes ownership of |exported_services|.
  ServiceProviderImpl* registry = exported_services.release();
  ServiceProviderPtr sp;
  if (registry) {
    BindToProxy(registry, &sp);
    imported_services.reset(registry->CreateRemoteServiceProvider());
  }
  static_cast<ViewManagerClientImpl*>(manager_)->Embed(url, id_, sp.Pass());
  return imported_services.Pass();
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

View::View()
    : manager_(NULL),
      id_(static_cast<Id>(-1)),
      parent_(NULL) {}

View::~View() {
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroying(this));
  if (parent_)
    parent_->LocalRemoveChild(this);
  // TODO(beng): It'd be better to do this via a destruction observer in the
  //             ViewManagerClientImpl.
  if (manager_)
    static_cast<ViewManagerClientImpl*>(manager_)->RemoveView(id_);
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewDestroyed(this));
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

View::View(ViewManager* manager)
    : manager_(manager),
      id_(static_cast<ViewManagerClientImpl*>(manager_)->CreateView()),
      parent_(NULL) {}

void View::LocalDestroy() {
  delete this;
}

void View::LocalAddChild(View* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
}

void View::LocalRemoveChild(View* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier notifier(child, this, NULL);
  RemoveChildImpl(child, &children_);
}

bool View::LocalReorder(View* relative, OrderDirection direction) {
  return ReorderImpl(&parent_->children_, this, relative, direction);
}

void View::LocalSetBounds(const gfx::Rect& old_bounds,
                          const gfx::Rect& new_bounds) {
  DCHECK(old_bounds == bounds_);
  ScopedSetBoundsNotifier notifier(this, old_bounds, new_bounds);
  bounds_ = new_bounds;
}

}  // namespace mojo
