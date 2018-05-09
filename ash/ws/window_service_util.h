// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_WINDOW_SERVICE_UTIL_H_
#define ASH_WS_WINDOW_SERVICE_UTIL_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "services/ui/ws2/window_service_delegate.h"

namespace service_manager {
class Service;
}  // namespace service_manager

namespace ui {
namespace ws2 {
class GpuSupport;
}  // namespace ws2
}  // namespace ui

namespace ash {

// A factory function to create WindowService with Ash-specific initialization.
// This and |gpu_support_factory| should only be run once, but factory functions
// for embedded mojo services use repeating callbacks.
ASH_EXPORT std::unique_ptr<service_manager::Service> CreateWindowService(
    const base::RepeatingCallback<std::unique_ptr<ui::ws2::GpuSupport>()>&
        gpu_support_factory);

}  // namespace ash

#endif  // ASH_WS_WINDOW_SERVICE_UTIL_H_
