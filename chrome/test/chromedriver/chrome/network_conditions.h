// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_

struct NetworkConditions {
  bool offline;
  double latency;
  double download_throughput;
  double upload_throughput;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_H_
