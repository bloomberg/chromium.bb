// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/window_tree_client_impl_private.h"

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"

namespace mus {

WindowTreeClientImplPrivate::WindowTreeClientImplPrivate(
    WindowTreeClientImpl* tree_client_impl)
    : tree_client_impl_(tree_client_impl) {}

WindowTreeClientImplPrivate::WindowTreeClientImplPrivate(Window* window)
    : WindowTreeClientImplPrivate(window->tree_client()) {}

WindowTreeClientImplPrivate::~WindowTreeClientImplPrivate() {}

uint32_t WindowTreeClientImplPrivate::event_observer_id() {
  return tree_client_impl_->event_observer_id_;
}

void WindowTreeClientImplPrivate::OnEmbed(mojom::WindowTree* window_tree) {
  mojom::WindowDataPtr root_data(mojom::WindowData::New());
  root_data->parent_id = 0;
  root_data->window_id = 1;
  root_data->bounds = mojo::Rect::From(gfx::Rect());
  root_data->properties.SetToEmpty();
  root_data->visible = true;
  root_data->viewport_metrics = mojom::ViewportMetrics::New();
  root_data->viewport_metrics->size_in_pixels =
      mojo::Size::From(gfx::Size(1000, 1000));
  root_data->viewport_metrics->device_pixel_ratio = 1;
  tree_client_impl_->OnEmbedImpl(window_tree, 1, std::move(root_data), 0, true);
}

void WindowTreeClientImplPrivate::CallOnWindowInputEvent(
    Window* window,
    const ui::Event& event) {
  const uint32_t event_id = 0u;
  const uint32_t observer_id = 0u;
  tree_client_impl_->OnWindowInputEvent(event_id, window->server_id(),
                                        mojom::Event::From(event), observer_id);
}

}  // namespace mus
