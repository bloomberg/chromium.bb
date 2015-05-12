// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surfaces/surfaces_service_application.h"
#include "mojo/application/application_runner_chromium.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(
      new surfaces::SurfacesServiceApplication);
  return runner.Run(shell_handle);
}
