// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"

// Responsible for layout of top level windows. Acts as an observer for the
// container and all the children of that container.
// TODO(beng): figure out what layout managers are needed for other containers.
class WindowLayout : public mus::WindowObserver {
 public:
  explicit WindowLayout(mus::Window* owner);
  ~WindowLayout() override;

 private:
  // Overridden from mus::WindowObserver:
  void OnTreeChanged(
      const mus::WindowObserver::TreeChangeParams& params) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const mojo::Rect& old_bounds,
                             const mojo::Rect& new_bounds) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  void LayoutWindow(mus::Window* window);
  void FitToContainer(mus::Window* window);
  void CenterWindow(mus::Window* window, const mojo::Size& preferred_size);

  mus::Window* owner_;

  DISALLOW_COPY_AND_ASSIGN(WindowLayout);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_
