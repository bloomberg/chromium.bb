// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_RASTER_THREAD_HELPER_H_
#define COMPONENTS_MUS_GLES2_RASTER_THREAD_HELPER_H_

#include "base/memory/scoped_ptr.h"

namespace cc {
class TaskGraphRunner;
}

namespace gles2 {

class RasterThreadHelper {
 public:
  RasterThreadHelper();
  ~RasterThreadHelper();

  cc::TaskGraphRunner* task_graph_runner() { return task_graph_runner_.get(); }

 private:
  class RasterThread;

  scoped_ptr<cc::TaskGraphRunner> task_graph_runner_;
  scoped_ptr<RasterThread> raster_thread_;

  DISALLOW_COPY_AND_ASSIGN(RasterThreadHelper);
};

}  // namespace gles2

#endif  // COMPONENTS_MUS_GLES2_RASTER_THREAD_HELPER_H_
