// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counters used within the browser.

#ifndef CONTENT_COMMON_CONTENT_COUNTERS_H_
#define CONTENT_COMMON_CONTENT_COUNTERS_H_
#pragma once

#include "content/common/content_export.h"

namespace base {
class StatsCounter;
class StatsCounterTimer;
class StatsRate;
}

namespace content {

class Counters {
 public:
  // The amount of time spent in chrome initialization.
  CONTENT_EXPORT static base::StatsCounterTimer& chrome_main();

  // The amount of time spent in renderer initialization.
  static base::StatsCounterTimer& renderer_main();
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_COUNTERS_H_
