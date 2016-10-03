// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/property_util.h"

#include "ash/wm/window_properties.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/mus_util.h"

namespace {

// Get the corresponding ui::Window property key for an aura::Window key.
template <typename T>
const char* GetMusProperty(const aura::WindowProperty<T>* aura_key) {
  if (aura_key == ash::kShelfIconResourceIdKey)
    return ui::mojom::WindowManager::kShelfIconResourceId_Property;
  if (aura_key == ash::kShelfItemTypeKey)
    return ui::mojom::WindowManager::kShelfItemType_Property;
  NOTREACHED();
  return nullptr;
}

}  // namespace

namespace property_util {

void SetIntProperty(aura::Window* window,
                    const aura::WindowProperty<int>* property,
                    int value) {
  DCHECK(window);
  if (chrome::IsRunningInMash()) {
    aura::GetMusWindow(window)->SetSharedProperty<int>(GetMusProperty(property),
                                                       value);
  } else {
    window->SetProperty(property, value);
  }
}

void SetTitle(aura::Window* window, const base::string16& value) {
  DCHECK(window);
  if (chrome::IsRunningInMash()) {
    aura::GetMusWindow(window)->SetSharedProperty<base::string16>(
        ui::mojom::WindowManager::kWindowTitle_Property, value);
  } else {
    window->SetTitle(value);
  }
}

}  // namespace property_util
