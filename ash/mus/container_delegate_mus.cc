// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/container_delegate_mus.h"

#include <stdint.h>

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"

using mash::wm::mojom::Container;

namespace ash {
namespace sysui {

namespace {

// Returns true if |widget| is in |container|.
bool IsInContainer(views::Widget* widget, Container container) {
  mus::Window* window =
      aura::GetMusWindow(widget->GetTopLevelWidget()->GetNativeWindow());
  if (window->HasSharedProperty(mash::wm::mojom::kWindowContainer_Property))
    return container ==
           static_cast<Container>(window->GetSharedProperty<int32_t>(
               mash::wm::mojom::kWindowContainer_Property));

  return false;
}

}  // namespace

ContainerDelegateMus::ContainerDelegateMus() {}

ContainerDelegateMus::~ContainerDelegateMus() {}

bool ContainerDelegateMus::IsInMenuContainer(views::Widget* widget) {
  return IsInContainer(widget, Container::MENUS);
}

bool ContainerDelegateMus::IsInStatusContainer(views::Widget* widget) {
  return IsInContainer(widget, Container::STATUS);
}

}  // namespace sysui
}  // namespace ash
