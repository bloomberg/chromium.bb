// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_SHELL_MEDIA_STREAM_CLIENT_H_
#define CONTENT_SHELL_RENDERER_SHELL_MEDIA_STREAM_CLIENT_H_

#include "base/callback_forward.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/renderer/media/media_stream_client.h"

namespace content {

// ShellMediaStreamClient is a mock implementation of
// webkit_media::MediaStreamClient used when running layout tests.
class ShellMediaStreamClient : public webkit_media::MediaStreamClient {
 public:
  ShellMediaStreamClient();
  virtual ~ShellMediaStreamClient();

  // webkit_media::MediaStreamClient implementation.
  virtual bool IsMediaStream(const GURL& url) OVERRIDE;
  virtual scoped_refptr<webkit_media::VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const webkit_media::VideoFrameProvider::RepaintCB& repaint_cb) OVERRIDE;
  virtual scoped_refptr<webkit_media::MediaStreamAudioRenderer>
      GetAudioRenderer(const GURL& url) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_SHELL_MEDIA_STREAM_CLIENT_H_
