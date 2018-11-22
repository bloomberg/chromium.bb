// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/c/main.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/mojo/services/media_service_factory.h"
#include "services/service_manager/public/mojom/service.mojom.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  std::unique_ptr<service_manager::Service> service =
      media::CreateMediaServiceForTesting(
          service_manager::mojom::ServiceRequest(mojo::ScopedMessagePipeHandle(
              mojo::MessagePipeHandle(service_request_handle))));
  service->set_termination_closure(run_loop.QuitClosure());
  run_loop.Run();
  return MOJO_RESULT_OK;
}
