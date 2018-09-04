// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_BROWSER_MAIN_EXTRA_PARTS_TRACING_H_
#define CHROME_BROWSER_TRACING_CHROME_BROWSER_MAIN_EXTRA_PARTS_TRACING_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "components/tracing/common/tracing_sampler_profiler.h"

class ChromeBrowserMainExtraPartsTracing : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsTracing();
  ~ChromeBrowserMainExtraPartsTracing() override;

  // MainMessageLoopRun methods.
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<tracing::TracingSamplerProfiler> sampler_profiler_;
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsTracing);
};

#endif  // CHROME_BROWSER_TRACING_CHROME_BROWSER_MAIN_EXTRA_PARTS_TRACING_H_
