// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_

#include <string>
#include <vector>

#include "media/base/media_log.h"
#include "third_party/WebKit/public/web/WebMediaSourceClient.h"

namespace media {
class ChunkDemuxer;
}

namespace content {

class WebMediaSourceClientImpl : public WebKit::WebMediaSourceClient {
 public:
  WebMediaSourceClientImpl(media::ChunkDemuxer* demuxer, media::LogCB log_cb);
  virtual ~WebMediaSourceClientImpl();

  // WebKit::WebMediaSourceClient implementation.
  virtual AddStatus addSourceBuffer(
      const WebKit::WebString& type,
      const WebKit::WebVector<WebKit::WebString>& codecs,
      WebKit::WebSourceBuffer** source_buffer) OVERRIDE;
  virtual double duration() OVERRIDE;
  virtual void setDuration(double duration) OVERRIDE;
  // TODO(acolwell): Remove this once endOfStream() is removed from Blink.
  virtual void endOfStream(EndOfStreamStatus status);
  virtual void markEndOfStream(EndOfStreamStatus status) OVERRIDE;
  virtual void unmarkEndOfStream() OVERRIDE;

 private:
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.
  media::LogCB log_cb_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaSourceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIASOURCECLIENT_IMPL_H_
