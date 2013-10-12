// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/micro_benchmark.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace cc {

MicroBenchmark::MicroBenchmark(const DoneCallback& callback)
    : callback_(callback), is_done_(false) {}

MicroBenchmark::~MicroBenchmark() {}

bool MicroBenchmark::IsDone() const {
  return is_done_;
}

void MicroBenchmark::DidUpdateLayers(LayerTreeHost* host) {}

void MicroBenchmark::NotifyDone(scoped_ptr<base::Value> result) {
  callback_.Run(result.Pass());
  is_done_ = true;
}

void MicroBenchmark::RunOnLayer(Layer* layer) {}

void MicroBenchmark::RunOnLayer(PictureLayer* layer) {}

}  // namespace cc
