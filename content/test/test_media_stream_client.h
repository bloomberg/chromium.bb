// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_MEDIA_STREAM_CLIENT_H_
#define CONTENT_TEST_TEST_MEDIA_STREAM_CLIENT_H_

#include "base/callback_forward.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/renderer/media/media_stream_client.h"

namespace content {

// TestMediaStreamClient is a mock implementation of
// webkit_media::MediaStreamClient used when running layout tests.
class TestMediaStreamClient : public RenderViewObserver,
                              public webkit_media::MediaStreamClient {
 public:
  explicit TestMediaStreamClient(RenderView* render_view);
  virtual ~TestMediaStreamClient();

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

#endif  // CONTENT_TEST_TEST_MEDIA_STREAM_CLIENT_H_
