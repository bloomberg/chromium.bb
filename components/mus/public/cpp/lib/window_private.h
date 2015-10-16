// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_

#include "components/mus/public/cpp/window.h"

namespace mus {

// This class is a friend of a View and contains functions to mutate internal
// state of View.
class WindowPrivate {
 public:
  explicit WindowPrivate(Window* view);
  ~WindowPrivate();

  // Creates and returns a new View. Caller owns the return value.
  static Window* LocalCreate();

  base::ObserverList<WindowObserver>* observers() {
    return &window_->observers_;
  }

  void ClearParent() { window_->parent_ = NULL; }

  void set_visible(bool visible) { window_->visible_ = visible; }

  void set_drawn(bool drawn) { window_->drawn_ = drawn; }

  void set_id(Id id) { window_->id_ = id; }

  void set_connection(WindowTreeConnection* connection) {
    window_->connection_ = connection;
  }

  void set_properties(const std::map<std::string, std::vector<uint8_t>>& data) {
    window_->properties_ = data;
  }

  void LocalSetViewportMetrics(const mojo::ViewportMetrics& old_metrics,
                               const mojo::ViewportMetrics& new_metrics) {
    window_->LocalSetViewportMetrics(new_metrics, new_metrics);
  }

  void LocalDestroy() { window_->LocalDestroy(); }
  void LocalAddChild(Window* child) { window_->LocalAddChild(child); }
  void LocalRemoveChild(Window* child) { window_->LocalRemoveChild(child); }
  void LocalReorder(Window* relative, mojo::OrderDirection direction) {
    window_->LocalReorder(relative, direction);
  }
  void LocalSetBounds(const mojo::Rect& old_bounds,
                      const mojo::Rect& new_bounds) {
    window_->LocalSetBounds(old_bounds, new_bounds);
  }
  void LocalSetClientArea(const mojo::Rect& new_client_area) {
    window_->LocalSetClientArea(new_client_area);
  }
  void LocalSetDrawn(bool drawn) { window_->LocalSetDrawn(drawn); }
  void LocalSetVisible(bool visible) { window_->LocalSetVisible(visible); }

 private:
  Window* window_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
