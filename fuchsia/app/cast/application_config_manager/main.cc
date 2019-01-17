// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "fuchsia/app/cast/application_config_manager/application_config_manager.h"

int main(int argc, char** argv) {
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  // Bind the parent-supplied ServiceDirectory-request to a directory and
  // publish the AppConfigManager service into it.
  base::fuchsia::ServiceDirectory* directory =
      base::fuchsia::ServiceDirectory::GetDefault();
  castrunner::ApplicationConfigManager app_config_manager;
  base::fuchsia::ScopedServiceBinding<
      ::chromium::cast::ApplicationConfigManager>
      binding(directory, &app_config_manager);

  // The main thread loop will be terminated when there are no more clients
  // connected to this service. The system service manager will restart the
  // service on demand as needed.
  binding.SetOnLastClientCallback(run_loop.QuitClosure());
  run_loop.Run();

  return 0;
}
