// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_MICRO_BENCHMARK_H_
#define CC_DEBUG_MICRO_BENCHMARK_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}  // namespace base

namespace cc {

class LayerTreeHost;
class Layer;
class PictureLayer;
class MicroBenchmark {
 public:
  typedef base::Callback<void(scoped_ptr<base::Value>)> DoneCallback;

  explicit MicroBenchmark(const DoneCallback& callback);
  virtual ~MicroBenchmark();

  bool IsDone() const;
  virtual void DidUpdateLayers(LayerTreeHost* host);

  virtual void RunOnLayer(Layer* layer);
  virtual void RunOnLayer(PictureLayer* layer);

 protected:
  void NotifyDone(scoped_ptr<base::Value> result);

 private:
  DoneCallback callback_;
  bool is_done_;
};

}  // namespace cc

#endif  // CC_DEBUG_MICRO_BENCHMARK_H_
