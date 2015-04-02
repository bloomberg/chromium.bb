// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/tracing/trace_data_sink.h"

#include "base/logging.h"
#include "mojo/common/data_pipe_utils.h"

using mojo::common::BlockingCopyFromString;

namespace tracing {
namespace {

const char kStart[] = "{\"traceEvents\":[";
const char kEnd[] = "]}";

}  // namespace

TraceDataSink::TraceDataSink(mojo::ScopedDataPipeProducerHandle pipe)
    : pipe_(pipe.Pass()), empty_(true) {
  BlockingCopyFromString(kStart, pipe_);
}

TraceDataSink::~TraceDataSink() {
  if (pipe_.is_valid())
    Flush();
  DCHECK(!pipe_.is_valid());
}

void TraceDataSink::AddChunk(const std::string& json) {
  if (!empty_)
    BlockingCopyFromString(",", pipe_);
  empty_ = false;
  BlockingCopyFromString(json, pipe_);
}

void TraceDataSink::Flush() {
  BlockingCopyFromString(kEnd, pipe_);
  pipe_.reset();
}

}  // namespace tracing
