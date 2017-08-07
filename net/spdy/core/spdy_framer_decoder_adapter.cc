// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/core/spdy_framer_decoder_adapter.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "net/spdy/platform/api/spdy_estimate_memory_usage.h"
#include "net/spdy/platform/api/spdy_ptr_util.h"
#include "net/spdy/platform/api/spdy_string_utils.h"

#if defined(COMPILER_GCC)
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __PRETTY_FUNCTION__, this)
#elif defined(COMPILER_MSVC)
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __FUNCSIG__, this)
#else
#define PRETTY_THIS SpdyStringPrintf("%s@%p ", __func__, this)
#endif

namespace net {

SpdyFramerDecoderAdapter::SpdyFramerDecoderAdapter() {
  DVLOG(1) << PRETTY_THIS;
}

SpdyFramerDecoderAdapter::~SpdyFramerDecoderAdapter() {
  DVLOG(1) << PRETTY_THIS;
}

void SpdyFramerDecoderAdapter::set_visitor(
    SpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
}

void SpdyFramerDecoderAdapter::set_debug_visitor(
    SpdyFramerDebugVisitorInterface* debug_visitor) {
  debug_visitor_ = debug_visitor;
}

void SpdyFramerDecoderAdapter::set_process_single_input_frame(bool v) {
  process_single_input_frame_ = v;
}

}  // namespace net
