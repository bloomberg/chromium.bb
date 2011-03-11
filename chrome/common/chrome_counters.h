// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counters used within the browser.

#ifndef CHROME_COMMON_CHROME_COUNTERS_H_
#define CHROME_COMMON_CHROME_COUNTERS_H_
#pragma once

namespace base {
class StatsCounter;
class StatsCounterTimer;
class StatsRate;
}

namespace chrome {

class Counters {
 public:
  // The amount of time spent in chrome initialization.
  static base::StatsCounterTimer& chrome_main();

  // The amount of time spent in renderer initialization.
  static base::StatsCounterTimer& renderer_main();

  // Time spent in spellchecker initialization.
  static base::StatsCounterTimer& spellcheck_init();

  // Time/Count of spellcheck lookups.
  static base::StatsRate& spellcheck_lookup();
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_COUNTERS_H_
