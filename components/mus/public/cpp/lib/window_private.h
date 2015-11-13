// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_

#include "components/mus/public/cpp/window.h"
#include "mojo/public/cpp/bindings/array.h"

namespace mus {

// This class is a friend of a Window and contains functions to mutate internal
// state of Window.
class WindowPrivate {
 public:
  explicit WindowPrivate(Window* window);
  ~WindowPrivate();

  // Creates and returns a new Window. Caller owns the return value.
  static Window* LocalCreate();

  base::ObserverList<WindowObserver>* observers() {
    return &window_->observers_;
  }

  void ClearParent() { window_->parent_ = nullptr; }

  void ClearTransientParent() { window_->transient_parent_ = nullptr; }

  void set_visible(bool visible) { window_->visible_ = visible; }

  void set_drawn(bool drawn) { window_->drawn_ = drawn; }

  void set_id(Id id) { window_->id_ = id; }

  void set_connection(WindowTreeConnection* connection) {
    window_->connection_ = connection;
  }

  void set_properties(const std::map<std::string, std::vector<uint8_t>>& data) {
    window_->properties_ = data;
  }

  void LocalSetViewportMetrics(const mojom::ViewportMetrics& old_metrics,
                               const mojom::ViewportMetrics& new_metrics) {
    window_->LocalSetViewportMetrics(new_metrics, new_metrics);
  }

  void LocalDestroy() { window_->LocalDestroy(); }
  void LocalAddChild(Window* child) { window_->LocalAddChild(child); }
  void LocalRemoveChild(Window* child) { window_->LocalRemoveChild(child); }
  void LocalAddTransientWindow(Window* child) {
    window_->LocalAddTransientWindow(child);
  }
  void LocalRemoveTransientWindow(Window* child) {
    window_->LocalRemoveTransientWindow(child);
  }
  void LocalReorder(Window* relative, mojom::OrderDirection direction) {
    window_->LocalReorder(relative, direction);
  }
  void LocalSetBounds(const gfx::Rect& old_bounds,
                      const gfx::Rect& new_bounds) {
    window_->LocalSetBounds(old_bounds, new_bounds);
  }
  void LocalSetClientArea(const gfx::Insets& new_client_area) {
    window_->LocalSetClientArea(new_client_area);
  }
  void LocalSetDrawn(bool drawn) { window_->LocalSetDrawn(drawn); }
  void LocalSetVisible(bool visible) { window_->LocalSetVisible(visible); }
  void LocalSetSharedProperty(const std::string& name,
                              mojo::Array<uint8_t> new_data);
  void LocalSetSharedProperty(const std::string& name,
                              const std::vector<uint8_t>* data) {
    window_->LocalSetSharedProperty(name, data);
  }
  void NotifyWindowStackingChanged() { window_->NotifyWindowStackingChanged(); }

 private:
  Window* window_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
