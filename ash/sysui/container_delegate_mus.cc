// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/container_delegate_mus.h"

#include <stdint.h>

#include "ash/public/interfaces/container.mojom.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace sysui {

namespace {

// Returns true if |widget| is in |container|.
bool IsInContainer(views::Widget* widget, mojom::Container container) {
  mus::Window* window =
      aura::GetMusWindow(widget->GetTopLevelWidget()->GetNativeWindow());
  if (window->HasSharedProperty(mojom::kWindowContainer_Property))
    return container ==
           static_cast<mojom::Container>(window->GetSharedProperty<int32_t>(
               mojom::kWindowContainer_Property));

  return false;
}

}  // namespace

ContainerDelegateMus::ContainerDelegateMus() {}

ContainerDelegateMus::~ContainerDelegateMus() {}

bool ContainerDelegateMus::IsInMenuContainer(views::Widget* widget) {
  return IsInContainer(widget, mojom::Container::MENUS);
}

bool ContainerDelegateMus::IsInStatusContainer(views::Widget* widget) {
  return IsInContainer(widget, mojom::Container::STATUS);
}

}  // namespace sysui
}  // namespace ash
