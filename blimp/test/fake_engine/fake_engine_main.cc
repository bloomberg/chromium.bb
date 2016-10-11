// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "blimp/test/fake_engine/fake_engine.h"
#include "blimp/test/fake_engine/proto/logging.grpc.pb.h"

#include "third_party/grpc/include/grpc++/grpc++.h"

namespace blimp {

// The Fake Engine currently after starting just sends a test log request.
int Main() {
  FakeEngine fake_engine;
  fake_engine.Start();

  Logging::Stub* logging_stub = fake_engine.GetLoggingStub();

  LogRequest request;
  LogResponse response;

  request.set_message("test log message");

  grpc::ClientContext context;
  grpc::Status status = logging_stub->Log(&context, request, &response);

  if (!status.ok()) {
    LOG(ERROR) << "Log RPC failed: " << status.error_code() << ": "
               << status.error_message();
    exit(-1);
  }
  LOG(INFO) << "Log RPC success";

  fake_engine.WaitForShutdown();
  return 0;
}

}  // namespace blimp

int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  return blimp::Main();
}
