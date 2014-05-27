// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_NETWORK_STATS_UPLOADER_H_
#define CHROME_BROWSER_METRICS_NETWORK_STATS_UPLOADER_H_

#include <string>

#include "base/basictypes.h"

class PrefService;
class PrefRegistrySimple;

// NetworkStatsUploader implements the collection of various network stats,
// which is done upon successful transmission of an UMA log.
class NetworkStatsUploader {
 public:
  NetworkStatsUploader();
  ~NetworkStatsUploader();

  // Collects and reports various network stats to external servers.
  void CollectAndReportNetworkStats();

 private:
  // The TCP/UDP echo server to collect network connectivity stats.
  std::string network_stats_server_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStatsUploader);
};

#endif  // CHROME_BROWSER_METRICS_NETWORK_STATS_UPLOADER_H_
