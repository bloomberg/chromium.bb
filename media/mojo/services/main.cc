// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_runner.h"

MojoResult MojoMain(MojoHandle mojo_handle) {
  // Create MojoMediaApplication and enable logging.
  mojo::ApplicationRunner runner(
      new media::MojoMediaApplication(true, media::MojoMediaClient::Create()));
  return runner.Run(mojo_handle);
}
