// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAME_PRODUCER_H_
#define NET_SPDY_SPDY_FRAME_PRODUCER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class SpdyFrame;

// An object which provides a SpdyFrame for writing. We pass these
// around instead of SpdyFrames since some frames have to be generated
// "just in time".
class NET_EXPORT_PRIVATE SpdyFrameProducer {
 public:
  SpdyFrameProducer();

  // Produces the frame to be written. Will be called at most once.
  virtual scoped_ptr<SpdyFrame> ProduceFrame() = 0;

  virtual ~SpdyFrameProducer();

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFrameProducer);
};

// A simple wrapper around a single SpdyFrame.
class NET_EXPORT_PRIVATE SimpleFrameProducer : public SpdyFrameProducer {
 public:
  explicit SimpleFrameProducer(scoped_ptr<SpdyFrame> frame);

  virtual ~SimpleFrameProducer();

  virtual scoped_ptr<SpdyFrame> ProduceFrame() OVERRIDE;

 private:
  scoped_ptr<SpdyFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFrameProducer);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAME_PRODUCER_H_
