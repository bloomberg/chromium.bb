// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_H_
#define CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/power_profiler/power_event.h"

namespace content {

typedef std::vector<PowerEvent> PowerEventVector;

// A class used to get power usage.
class PowerDataProvider {
 public:
  static scoped_ptr<PowerDataProvider> Create();

  PowerDataProvider() {}
  virtual ~PowerDataProvider() {}

  // Returns a vector of power events, one per type, for the types it supports.
  virtual PowerEventVector GetData() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_PROFILER_POWER_DATA_PROVIDER_H_
