// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_FACTORY_H_
#define COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_FACTORY_H_

#include "components/view_manager/gles2/gpu_state.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"

namespace mojo {
class ApplicationImpl;
}  // namespace mojo

namespace view_manager {

class DisplayManager;

// Abstract factory for DisplayManagers. Used by tests to construct test
// DisplayManagers.
class DisplayManagerFactory {
 public:
  virtual DisplayManager* CreateDisplayManager(
      bool is_headless,
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<gles2::GpuState>& gpu_state) = 0;
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_DISPLAY_MANAGER_FACTORY_H_
