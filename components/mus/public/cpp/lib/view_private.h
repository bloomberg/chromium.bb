// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_PRIVATE_H_

#include "components/mus/public/cpp/view.h"

namespace mus {

// This class is a friend of a View and contains functions to mutate internal
// state of View.
class ViewPrivate {
 public:
  explicit ViewPrivate(View* view);
  ~ViewPrivate();

  // Creates and returns a new View. Caller owns the return value.
  static View* LocalCreate();

  base::ObserverList<ViewObserver>* observers() { return &view_->observers_; }

  void ClearParent() { view_->parent_ = NULL; }

  void set_visible(bool visible) { view_->visible_ = visible; }

  void set_drawn(bool drawn) { view_->drawn_ = drawn; }

  void set_id(Id id) { view_->id_ = id; }

  void set_connection(ViewTreeConnection* connection) {
    view_->connection_ = connection;
  }

  void set_properties(const std::map<std::string, std::vector<uint8_t>>& data) {
    view_->properties_ = data;
  }

  void LocalSetViewportMetrics(const mojo::ViewportMetrics& old_metrics,
                               const mojo::ViewportMetrics& new_metrics) {
    view_->LocalSetViewportMetrics(new_metrics, new_metrics);
  }

  void LocalDestroy() { view_->LocalDestroy(); }
  void LocalAddChild(View* child) { view_->LocalAddChild(child); }
  void LocalRemoveChild(View* child) { view_->LocalRemoveChild(child); }
  void LocalReorder(View* relative, mojo::OrderDirection direction) {
    view_->LocalReorder(relative, direction);
  }
  void LocalSetBounds(const mojo::Rect& old_bounds,
                      const mojo::Rect& new_bounds) {
    view_->LocalSetBounds(old_bounds, new_bounds);
  }
  void LocalSetDrawn(bool drawn) { view_->LocalSetDrawn(drawn); }
  void LocalSetVisible(bool visible) { view_->LocalSetVisible(visible); }

 private:
  View* view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_VIEW_PRIVATE_H_
