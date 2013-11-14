// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebSourceBuffer.h"

namespace media {
class ChunkDemuxer;
}

namespace content {

class WebSourceBufferImpl : public blink::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id, media::ChunkDemuxer* demuxer);
  virtual ~WebSourceBufferImpl();

  // blink::WebSourceBuffer implementation.
  virtual blink::WebTimeRanges buffered() OVERRIDE;
  virtual void append(const unsigned char* data, unsigned length) OVERRIDE;
  virtual void abort() OVERRIDE;
  // TODO(acolwell): Add OVERRIDE when Blink-side changes land.
  virtual void remove(double start, double end);
  virtual bool setTimestampOffset(double offset) OVERRIDE;
  // TODO(acolwell): Add OVERRIDE when Blink-side changes land.
  virtual void setAppendWindowStart(double start);
  virtual void setAppendWindowEnd(double end);
  virtual void removedFromMediaSource() OVERRIDE;

 private:
  std::string id_;
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_
