// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_frame_producer.h"

#include "base/logging.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyFrameProducer::SpdyFrameProducer() {}

SpdyFrameProducer::~SpdyFrameProducer() {}

SimpleFrameProducer::SimpleFrameProducer(scoped_ptr<SpdyFrame> frame)
    : frame_(frame.Pass()) {}

SimpleFrameProducer::~SimpleFrameProducer() {}

scoped_ptr<SpdyFrame> SimpleFrameProducer::ProduceFrame() {
  DCHECK(frame_);
  return frame_.Pass();
}

}  // namespace net
