// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_connection.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl)
#endif
#define VIEW_MANAGER_EXPORT __declspec(dllexport)
#else
#define CDECL
#define VIEW_MANAGER_EXPORT __attribute__((visibility("default")))
#endif

extern "C" VIEW_MANAGER_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;
  mojo::Application app(shell_handle);
  mojo::services::view_manager::RootNodeManager root_node_manager;
  app.AddServiceConnector(new mojo::ServiceConnector
                          <mojo::services::view_manager::ViewManagerConnection,
                           mojo::services::view_manager::RootNodeManager>(
                               &root_node_manager));
  loop.Run();

  return MOJO_RESULT_OK;
}
