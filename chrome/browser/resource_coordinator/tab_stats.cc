// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "build/build_config.h"

namespace resource_coordinator {

TabStats::TabStats() = default;

TabStats::TabStats(const TabStats& other) = default;

TabStats::TabStats(TabStats&& other) noexcept = default;

TabStats::~TabStats() {}

TabStats& TabStats::operator=(const TabStats& other) = default;

}  // namespace resource_coordinator
