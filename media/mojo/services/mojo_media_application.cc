// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "mojo/application/application_runner_chromium.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/interface_factory_impl.h"

namespace media {

class MojoMediaApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::MediaRenderer> {
 public:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override {
    base::CommandLine::StringVector command_line_args;
#if defined(OS_WIN)
    for (const auto& arg : app->args())
      command_line_args.push_back(base::UTF8ToUTF16(arg));
#elif defined(OS_POSIX)
    command_line_args = app->args();
#endif

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv(command_line_args);

    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::InitLogging(settings);
    // Display process ID, thread ID and timestamp in logs.
    logging::SetLogItems(true, true, true, false);
  }

  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::MediaRenderer> request) override {
    mojo::BindToRequest(new MojoRendererService(), &request);
  }
};

}  // namespace media

MojoResult MojoMain(MojoHandle mojo_handle) {
  mojo::ApplicationRunnerChromium runner(new media::MojoMediaApplication());
  return runner.Run(mojo_handle);
}
