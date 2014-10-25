// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/view.h"

namespace mojo {

// This class is a friend of a View and contains functions to mutate internal
// state of View.
class ViewPrivate {
 public:
  explicit ViewPrivate(View* view);
  ~ViewPrivate();

  // Creates and returns a new View. Caller owns the return value.
  static View* LocalCreate();

  ObserverList<ViewObserver>* observers() { return &view_->observers_; }

  void ClearParent() { view_->parent_ = NULL; }

  void set_visible(bool visible) { view_->visible_ = visible; }

  void set_drawn(bool drawn) { view_->drawn_ = drawn; }

  void set_id(Id id) { view_->id_ = id; }

  ViewManager* view_manager() { return view_->manager_; }
  void set_view_manager(ViewManager* manager) {
    view_->manager_ = manager;
  }

  void set_properties(const std::map<std::string, std::vector<uint8_t>>& data) {
    view_->properties_ = data;
  }

  void LocalDestroy() {
    view_->LocalDestroy();
  }
  void LocalAddChild(View* child) {
    view_->LocalAddChild(child);
  }
  void LocalRemoveChild(View* child) {
    view_->LocalRemoveChild(child);
  }
  void LocalReorder(View* relative, OrderDirection direction) {
    view_->LocalReorder(relative, direction);
  }
  void LocalSetBounds(const Rect& old_bounds,
                      const Rect& new_bounds) {
    view_->LocalSetBounds(old_bounds, new_bounds);
  }
  void LocalSetDrawn(bool drawn) { view_->LocalSetDrawn(drawn); }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewPrivate);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_PRIVATE_H_
