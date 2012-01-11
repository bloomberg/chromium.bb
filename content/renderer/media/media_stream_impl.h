// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "webkit/media/media_stream_client.h"

class VideoCaptureImplManager;

// A implementation of StreamClient to provide supporting functions, such as
// GetVideoDecoder.
class MediaStreamImpl
    : public webkit_media::MediaStreamClient,
      public base::RefCountedThreadSafe<MediaStreamImpl> {
 public:
  explicit MediaStreamImpl(VideoCaptureImplManager* vc_manager);
  virtual ~MediaStreamImpl();

  // Implement webkit_media::StreamClient.
  virtual scoped_refptr<media::VideoDecoder> GetVideoDecoder(
      const GURL& url,
      media::MessageLoopFactory* message_loop_factory) OVERRIDE;

 private:
  scoped_refptr<VideoCaptureImplManager> vc_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_IMPL_H_
