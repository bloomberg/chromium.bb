// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_stats_uploader.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "chrome/browser/net/network_stats.h"
#include "chrome/common/net/test_server_locations.h"

#if !defined(OS_POSIX)
#include "chrome/installer/util/browser_distribution.h"
#endif

NetworkStatsUploader::NetworkStatsUploader() {
#if defined(OS_POSIX)
  network_stats_server_ = chrome_common_net::kEchoTestServerLocation;
#else
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  network_stats_server_ = dist->GetNetworkStatsServer();
#endif
}

NetworkStatsUploader::~NetworkStatsUploader() {
}

void NetworkStatsUploader::CollectAndReportNetworkStats() {
  IOThread* io_thread = g_browser_process->io_thread();
  if (!io_thread)
    return;

  chrome_browser_net::CollectNetworkStats(network_stats_server_, io_thread);
#if defined(OS_WIN)
  chrome::CollectTimeTicksStats();
#endif
}
