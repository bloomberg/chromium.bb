// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"

namespace ui {
class Event;
}

namespace mus {
namespace mojom {
class WindowTree;
}

class Window;
class WindowTreeClient;

// Use to access implementation details of WindowTreeClient.
class WindowTreeClientPrivate {
 public:
  explicit WindowTreeClientPrivate(WindowTreeClient* tree_client_impl);
  explicit WindowTreeClientPrivate(Window* window);
  ~WindowTreeClientPrivate();

  uint32_t event_observer_id();

  // Calls OnEmbed() on the WindowTreeClient.
  void OnEmbed(mojom::WindowTree* window_tree);

  // Pretends that |event| has been received from the window server.
  void CallOnWindowInputEvent(Window* window, std::unique_ptr<ui::Event> event);

 private:
   WindowTreeClient* tree_client_impl_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_
