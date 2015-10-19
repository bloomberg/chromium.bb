// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/blink/skcanvas_video_renderer.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace blink {
class WebFrame;
class WebGraphicsContext3D;
class WebMediaPlayerClient;
}

namespace media {
class MediaLog;
class WebMediaPlayerDelegate;
class VideoFrame;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamRendererFactory;
class VideoFrameProvider;
class WebMediaPlayerMSCompositor;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// All calls to WebMediaPlayerMS methods must be from the main thread of
// Renderer process.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// VideoFrameProvider
//   provides video frames for rendering.
//
// TODO(wjia): add AudioPlayer.
// AudioPlayer
//   plays audio streams.
//
// blink::WebMediaPlayerClient
//   WebKit client of this media player object.
class WebMediaPlayerMS
    : public blink::WebMediaPlayer,
      public base::SupportsWeakPtr<WebMediaPlayerMS> {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides VideoFrameProvider.
  WebMediaPlayerMS(
      blink::WebFrame* frame,
      blink::WebMediaPlayerClient* client,
      base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
      media::MediaLog* media_log,
      scoped_ptr<MediaStreamRendererFactory> factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  ~WebMediaPlayerMS() override;

  void load(LoadType load_type,
            const blink::WebURL& url,
            CORSMode cors_mode) override;

  // Playback controls.
  void play() override;
  void pause() override;
  bool supportsSave() const override;
  void seek(double seconds) override;
  void setRate(double rate) override;
  void setVolume(double volume) override;
  void setSinkId(const blink::WebString& sink_id,
                 media::WebSetSinkIdCB* web_callback) override;
  void setPreload(blink::WebMediaPlayer::Preload preload) override;
  blink::WebTimeRanges buffered() const override;
  blink::WebTimeRanges seekable() const override;

  // Methods for painting.
  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             unsigned char alpha,
             SkXfermode::Mode mode) override;

  // True if the loaded media has a playable video/audio track.
  bool hasVideo() const override;
  bool hasAudio() const override;

  // Dimensions of the video.
  blink::WebSize naturalSize() const override;

  // Getters of playback state.
  bool paused() const override;
  bool seeking() const override;
  double duration() const override;
  double currentTime() const override;

  // Internal states of loading and network.
  blink::WebMediaPlayer::NetworkState networkState() const override;
  blink::WebMediaPlayer::ReadyState readyState() const override;

  bool didLoadingProgress() override;

  bool hasSingleSecurityOrigin() const override;
  bool didPassCORSAccessCheck() const override;

  double mediaTimeForTimeValue(double timeValue) const override;

  unsigned decodedFrameCount() const override;
  unsigned droppedFrameCount() const override;
  unsigned audioDecodedByteCount() const override;
  unsigned videoDecodedByteCount() const override;

  bool copyVideoTextureToPlatformTexture(
      blink::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y) override;

 private:
  // The callback for VideoFrameProvider to signal a new frame is available.
  void OnFrameAvailable(const scoped_refptr<media::VideoFrame>& frame);
  // Need repaint due to state change.
  void RepaintInternal();

  // The callback for source to report error.
  void OnSourceError();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  blink::WebMediaPlayerClient* get_client() { return client_; }

  blink::WebFrame* const frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  const blink::WebTimeRanges buffered_;

  blink::WebMediaPlayerClient* const client_;

  const base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;

  // Specify content:: to disambiguate from cc::.
  scoped_refptr<content::VideoFrameProvider> video_frame_provider_;  // Weak

  scoped_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;  // Weak
  media::SkCanvasVideoRenderer video_renderer_;

  bool paused_;
  bool received_first_frame_;

  scoped_refptr<media::MediaLog> media_log_;

  scoped_ptr<MediaStreamRendererFactory> renderer_factory_;

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::TaskRunner> worker_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  // WebMediaPlayerMS owns |compositor_| and destroys it on
  // |compositor_task_runner_|.
  scoped_ptr<WebMediaPlayerMSCompositor> compositor_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
