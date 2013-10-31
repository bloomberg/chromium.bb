// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_CONTROLLER_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_CONTROLLER_H_

#include <string>

#include "mojo/public/system/core.h"
#include "mojo/services/native_viewport/native_viewport.h"

namespace mojo {
namespace services {

class NativeViewportController : public services::NativeViewportDelegate {
 public:
  // TODO(beng): Currently, pipe is just the single pipe that exists between
  //             mojo_shell and the loaded app. This should really be hidden
  //             behind the bindings layer, when that comes up.
  explicit NativeViewportController(Handle pipe);
  virtual ~NativeViewportController();

  void Close();

 private:
  // Overridden from services::NativeViewportDelegate:
  virtual bool OnEvent(ui::Event* event) OVERRIDE;
  virtual void OnResized(const gfx::Size& size) OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;

  void SendString(const std::string& string);

  Handle pipe_;
  scoped_ptr<NativeViewport> native_viewport_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportController);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_CONTROLLER_H_
