// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_

#include "base/trace_event/trace_event_impl.h"
#include "content/common/content_export.h"

namespace base {
class DictionaryValue;
}

namespace content {

// BackgroundTracingConfig is passed to the BackgroundTracingManager to
// setup the trigger rules used to enable/disable background tracing.
struct CONTENT_EXPORT BackgroundTracingConfig {
  virtual ~BackgroundTracingConfig();

  enum Mode {
    PREEMPTIVE_TRACING_MODE,
    REACTIVE_TRACING_MODE,
  };
  enum CategoryPreset {
    BENCHMARK,
    BENCHMARK_DEEP,
    BENCHMARK_GPU,
    BENCHMARK_IPC
  };

  Mode mode;

  static scoped_ptr<BackgroundTracingConfig> FromDict(
      const base::DictionaryValue* dict);
  static void IntoDict(const BackgroundTracingConfig* config,
                       base::DictionaryValue* dict);

 protected:
  explicit BackgroundTracingConfig(Mode mode);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
