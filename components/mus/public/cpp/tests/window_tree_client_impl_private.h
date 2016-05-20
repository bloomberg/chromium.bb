// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_IMPL_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_IMPL_PRIVATE_H_

#include <stdint.h>

#include "base/macros.h"

namespace ui {
class Event;
}

namespace mus {
namespace mojom {
class WindowTree;
}

class Window;
class WindowTreeClientImpl;

// Use to access implementation details of WindowTreeClientImpl.
class WindowTreeClientImplPrivate {
 public:
  explicit WindowTreeClientImplPrivate(WindowTreeClientImpl* tree_client_impl);
  explicit WindowTreeClientImplPrivate(Window* window);
  ~WindowTreeClientImplPrivate();

  uint32_t event_observer_id();

  // Calls OnEmbed() on the WindowTreeClientImpl.
  void OnEmbed(mojom::WindowTree* window_tree);

  // Pretends that |event| has been received from the window server.
  void CallOnWindowInputEvent(Window* window, const ui::Event& event);

 private:
  WindowTreeClientImpl* tree_client_impl_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImplPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_IMPL_PRIVATE_H_
