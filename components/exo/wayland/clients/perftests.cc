// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/exo/wayland/clients/simple.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "testing/perf/perf_test.h"

namespace {

using WaylandClientPerfTests = exo::WaylandClientTest;

TEST_F(WaylandClientPerfTests, Simple) {
  const int kWarmUpFrames = 20;
  const int kTestFrames = 600;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  EXPECT_TRUE(params.FromCommandLine(*command_line));

  exo::wayland::clients::Simple client;
  EXPECT_TRUE(client.Init(params));

  client.Run(kWarmUpFrames);

  auto start_time = base::Time::Now();
  client.Run(kTestFrames);
  auto time_delta = base::Time::Now() - start_time;
  float fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "Simple", fps,
                         "frames/s", true);
}

}  // namespace
