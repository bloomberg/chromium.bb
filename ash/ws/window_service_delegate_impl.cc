// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_delegate_impl.h"

#include "ash/wm/container_finder.h"
#include "ash/wm/top_level_window_factory.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/window.h"

namespace ash {

WindowServiceDelegateImpl::WindowServiceDelegateImpl() = default;

WindowServiceDelegateImpl::~WindowServiceDelegateImpl() = default;

std::unique_ptr<aura::Window> WindowServiceDelegateImpl::NewTopLevel(
    aura::PropertyConverter* property_converter,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::map<std::string, std::vector<uint8_t>> property_map =
      mojo::FlatMapToMap(properties);
  ui::mojom::WindowType window_type = ui::mojom::WindowType::UNKNOWN;
  auto type_iter =
      property_map.find(ui::mojom::WindowManager::kWindowType_InitProperty);
  if (type_iter != property_map.end()) {
    window_type = static_cast<ui::mojom::WindowType>(
        mojo::ConvertTo<int32_t>(type_iter->second));
  }

  auto* window =
      CreateAndParentTopLevelWindow(nullptr /* window_manager */, window_type,
                                    property_converter, &property_map);
  return base::WrapUnique<aura::Window>(window);
}

}  // namespace ash
