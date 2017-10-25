// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/exo/wayland/clients/blur.h"
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

TEST_F(WaylandClientPerfTests, Blur) {
  const int kWarmUpFrames = 20;
  const int kTestFrames = 600;
  const double kMaxSigma = 4.0;
  const bool kOffscreen = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  EXPECT_TRUE(params.FromCommandLine(*command_line));

  exo::wayland::clients::Blur client;
  EXPECT_TRUE(client.Init(params));

  client.Run(0, 0, 0, false, kWarmUpFrames);

  auto start_time = base::Time::Now();
  client.Run(0, 0, kMaxSigma, kOffscreen, kTestFrames);
  auto time_delta = base::Time::Now() - start_time;
  float fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0x0", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 5, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0x5", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 10, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0x10", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 25, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0x25", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 50, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0x50", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(5, 5, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma5x5", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(15, 15, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma15x15", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(30, 30, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma30x30", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(50, 50, kMaxSigma, kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma50x50", fps,
                         "frames/s", true);
}

}  // namespace
