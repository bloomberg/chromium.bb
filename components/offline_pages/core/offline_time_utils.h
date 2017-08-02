// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_TIME_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_TIME_UTILS_H_

#include <stdint.h>

namespace base {
class Time;
}

namespace offline_pages {

// Time conversion methods for the time format recommended for offline pages
// projects for database storage: elapsed time in microseconds since the Windows
// epoch. Not all offline pages projects adhere to this format for legacy
// reasons.
int64_t ToDatabaseTime(base::Time time);
base::Time FromDatabaseTime(int64_t serialized_time);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_TIME_UTILS_H_
