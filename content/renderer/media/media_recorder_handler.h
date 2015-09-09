// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandlerClient.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"

namespace blink {
class WebString;
}  // namespace blink

namespace media {
class VideoFrame;
class WebmMuxer;
}  // namespace media

namespace content {

class VideoTrackRecorder;

// MediaRecorderHandler orchestrates the creation, lifetime mgmt and mapping
// between:
// - MediaStreamTrack(s) providing data,
// - {Audio,Video}TrackRecorders encoding that data,
// - a WebmMuxer class multiplexing encoded data into a WebM container, and
// - a single recorder client receiving this contained data.
// All methods are called on the same thread as construction and destruction,
// i.e. the Main Render thread.
// TODO(mcasas): Implement audio recording.
class CONTENT_EXPORT MediaRecorderHandler final {
 public:
  MediaRecorderHandler();
  ~MediaRecorderHandler();

  // See above, these methods should override blink::WebMediaRecorderHandler.
  bool canSupportMimeType(const blink::WebString& mimeType);
  bool initialize(blink::WebMediaRecorderHandlerClient* client,
                  const blink::WebMediaStream& media_stream,
                  const blink::WebString& mimeType);
  bool start();
  bool start(int timeslice);
  void stop();
  void pause();
  void resume();

 private:
  friend class MediaRecorderHandlerTest;

  void WriteData(const base::StringPiece& data);

  void OnVideoFrameForTesting(const scoped_refptr<media::VideoFrame>& frame,
                              const base::TimeTicks& timestamp);

  // Bound to the main render thread.
  base::ThreadChecker main_render_thread_checker_;

  bool recording_;
  blink::WebMediaStream media_stream_;  // The MediaStream being recorded.

  // |client_| is a weak pointer, and is valid for the lifetime of this object.
  blink::WebMediaRecorderHandlerClient* client_;

  ScopedVector<VideoTrackRecorder> video_recorders_;

  // Worker class doing the actual Webm Muxing work.
  scoped_ptr<media::WebmMuxer> webm_muxer_;

  base::WeakPtrFactory<MediaRecorderHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRecorderHandler);
};

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_MEDIA_RECORDER_HANDLER_H_
