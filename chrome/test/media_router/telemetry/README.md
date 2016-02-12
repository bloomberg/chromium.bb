<!-- Copyright 2016 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# How to run benchmarks for media router
1. Run the following command to find all the avaiable browsers:
./chrome/test/media_router/telemetry/run_benchmark --browser list

2. Run the following command to get benchmarks for media router dialog latency:
./chrome/test/media_router/telemetry/run_benchmark --browser=<one of the values returned in step 1> media_router.dialog.latency.tracing --reset-results

./chrome/test/media_router/telemetry/run_benchmark --browser=<one of the values returned in step 1> media_router.dialog.latency.histogram

The results will be in <chromium src folder>/chrome/test/media_router/telemetry/results.html
