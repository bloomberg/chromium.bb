// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"

namespace mojo {

class WindowManagerApp;

class WindowManagerServiceImpl : public InterfaceImpl<WindowManagerService> {
 public:
  explicit WindowManagerServiceImpl(WindowManagerApp* manager);
  virtual ~WindowManagerServiceImpl();

  void NotifyReady();
  void NotifyNodeFocused(Id new_focused_id, Id old_focused_id);
  void NotifyWindowActivated(Id new_active_id, Id old_active_id);

 private:
  // Overridden from WindowManagerService:
  virtual void OpenWindow(const Callback<void(Id)>& callback) MOJO_OVERRIDE;
  virtual void OpenWindowWithURL(
      const String& url,
      const Callback<void(Id)>& callback) MOJO_OVERRIDE;
  virtual void SetCapture(Id node,
                          const Callback<void(bool)>& callback) MOJO_OVERRIDE;
  virtual void FocusWindow(Id node,
                           const Callback<void(bool)>& callback) MOJO_OVERRIDE;
  virtual void ActivateWindow(
      Id node,
      const Callback<void(bool)>& callback) MOJO_OVERRIDE;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  WindowManagerApp* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_
