// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/dbus_echo/echo.mojom.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define DBUS_ECHO_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define DBUS_ECHO_APP_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class DBusEchoApp : public Application {
 public:
  explicit DBusEchoApp(MojoHandle shell_handle) : Application(shell_handle) {
    ConnectTo("dbus:org.chromium.EchoService/org/chromium/MojoImpl",
              &echo_service_);

    AllocationScope scope;
    echo_service_->Echo("who", base::Bind(&DBusEchoApp::OnEcho,
                                          base::Unretained(this)));
  }

  virtual ~DBusEchoApp() {
  }

 private:
  void OnEcho(const String& echoed) {
    LOG(INFO) << "echo'd " << echoed.To<std::string>();
  }

  EchoServicePtr echo_service_;
};

}  // namespace examples
}  // namespace mojo

extern "C" DBUS_ECHO_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;

  mojo::examples::DBusEchoApp app(shell_handle);
  loop.Run();
  return MOJO_RESULT_OK;
}
