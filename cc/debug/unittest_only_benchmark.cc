// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "cc/debug/unittest_only_benchmark.h"

namespace cc {

UnittestOnlyBenchmark::UnittestOnlyBenchmark(scoped_ptr<base::Value> value,
                                             const DoneCallback& callback)
    : MicroBenchmark(callback) {}

UnittestOnlyBenchmark::~UnittestOnlyBenchmark() {}

void UnittestOnlyBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  NotifyDone(scoped_ptr<base::Value>());
}

}  // namespace cc
