// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CALLBACK_UTIL_H_
#define MEDIA_BASE_CALLBACK_UTIL_H_

#include <queue>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/pipeline_status.h"

namespace media {

typedef base::Callback<void(const base::Closure&)> ClosureFunc;
typedef base::Callback<void(const PipelineStatusCB&)> PipelineStatusCBFunc;

// Executes the closures in FIFO order, executing |done_cb| when the last
// closure has completed running.
//
// All closures (including |done_cb|) are executed on same thread as the
// calling thread.
void RunInSeries(scoped_ptr<std::queue<ClosureFunc> > closures,
                 const base::Closure& done_cb);

// Executes the closures in FIFO order, executing |done_cb| when the last
// closure has completed running, reporting the final status code.
//
// Closures will stop being executed if a previous closure in the series
// returned an error status and |done_cb| will be executed prematurely.
//
// All closures (including |done_cb|) are executed on same thread as the
// calling thread.
void RunInSeriesWithStatus(
    scoped_ptr<std::queue<PipelineStatusCBFunc> > status_cbs,
    const PipelineStatusCB& done_cb);

// Executes the closures in parallel, executing |done_cb| when all closures have
// completed running.
//
// No attempt is made to parallelize execution of the closures. In other words,
// this method will run all closures in FIFO order if said closures execute
// synchronously on the same call stack.
//
// All closures (including |done_cb|) are executed on same thread as the
// calling thread.
void RunInParallel(scoped_ptr<std::queue<ClosureFunc> > closures,
                   const base::Closure& done_cb);

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_UTIL_H_
