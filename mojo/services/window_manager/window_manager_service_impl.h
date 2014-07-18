// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"

namespace mojo {

class ApplicationConnection;
class WindowManagerApp;

class WindowManagerServiceImpl : public InterfaceImpl<WindowManagerService> {
 public:
  WindowManagerServiceImpl(ApplicationConnection* connection,
                           WindowManagerApp* manager);
  virtual ~WindowManagerServiceImpl();

  void NotifyReady();
  void NotifyNodeFocused(view_manager::Id new_focused_id,
                         view_manager::Id old_focused_id);
  void NotifyWindowActivated(view_manager::Id new_active_id,
                             view_manager::Id old_active_id);

 private:
  // Overridden from WindowManagerService:
  virtual void OpenWindow(
      const Callback<void(view_manager::Id)>& callback) MOJO_OVERRIDE;
  virtual void OpenWindowWithURL(
      const String& url,
      const Callback<void(view_manager::Id)>& callback) MOJO_OVERRIDE;
  virtual void SetCapture(view_manager::Id node,
                          const Callback<void(bool)>& callback) MOJO_OVERRIDE;
  virtual void FocusWindow(view_manager::Id node,
                           const Callback<void(bool)>& callback) MOJO_OVERRIDE;
  virtual void ActivateWindow(
      view_manager::Id node,
      const Callback<void(bool)>& callback) MOJO_OVERRIDE;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;

  WindowManagerApp* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE_IMPL_H_
