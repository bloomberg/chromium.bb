// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_
#define CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_

#include "cc/debug/micro_benchmark.h"

namespace cc {

class UnittestOnlyBenchmark : public MicroBenchmark {
 public:
  explicit UnittestOnlyBenchmark(scoped_ptr<base::Value> value,
                                 const DoneCallback& callback);
  virtual ~UnittestOnlyBenchmark();

  virtual void DidUpdateLayers(LayerTreeHost* host) OVERRIDE;
};

}  // namespace cc

#endif  // CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_

