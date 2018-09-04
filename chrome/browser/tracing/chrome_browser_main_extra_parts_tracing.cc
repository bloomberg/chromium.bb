// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_browser_main_extra_parts_tracing.h"

ChromeBrowserMainExtraPartsTracing::ChromeBrowserMainExtraPartsTracing() =
    default;
ChromeBrowserMainExtraPartsTracing::~ChromeBrowserMainExtraPartsTracing() =
    default;

void ChromeBrowserMainExtraPartsTracing::PreMainMessageLoopRun() {
  sampler_profiler_ = std::make_unique<tracing::TracingSamplerProfiler>();
}

void ChromeBrowserMainExtraPartsTracing::PostMainMessageLoopRun() {
  sampler_profiler_.reset();
}
