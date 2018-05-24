// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_H_
#define CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_stream.h"

namespace blink {
class WebLocalFrame;
class WebMediaPlayerClient;
class WebString;
}

namespace media {
class GpuMemoryBufferVideoFramePool;
class MediaLog;
}

namespace cc {
class VideoLayer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamRendererFactory;
class MediaStreamVideoRenderer;
class WebMediaPlayerMSCompositor;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// All calls to WebMediaPlayerMS methods must be from the main thread of
// Renderer process.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// MediaStreamVideoRenderer
//   provides video frames for rendering.
//
// blink::WebMediaPlayerClient
//   WebKit client of this media player object.
class CONTENT_EXPORT WebMediaPlayerMS
    : public blink::WebMediaStreamObserver,
      public blink::WebMediaPlayer,
      public media::WebMediaPlayerDelegate::Observer,
      public base::SupportsWeakPtr<WebMediaPlayerMS> {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides MediaStreamVideoRenderer.
  // |delegate| must not be null.
  WebMediaPlayerMS(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      media::WebMediaPlayerDelegate* delegate,
      std::unique_ptr<media::MediaLog> media_log,
      std::unique_ptr<MediaStreamRendererFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::TaskRunner> worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const blink::WebString& sink_id);

  ~WebMediaPlayerMS() override;

  void Load(LoadType load_type,
            const blink::WebMediaPlayerSource& source,
            CORSMode cors_mode) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;
  void EnterPictureInPicture(
      blink::WebMediaPlayer::PipWindowOpenedCallback callback) override;
  void ExitPictureInPicture(
      blink::WebMediaPlayer::PipWindowClosedCallback callback) override;
  void RegisterPictureInPictureWindowResizeCallback(
      blink::WebMediaPlayer::PipWindowResizedCallback) override;
  void SetSinkId(const blink::WebString& sink_id,
                 blink::WebSetSinkIdCallbacks* web_callback) override;
  void SetPreload(blink::WebMediaPlayer::Preload preload) override;
  blink::WebTimeRanges Buffered() const override;
  blink::WebTimeRanges Seekable() const override;

  // Methods for painting.
  void Paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             cc::PaintFlags& flags,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override;
  media::PaintCanvasVideoRenderer* GetPaintCanvasVideoRenderer();
  void ResetCanvasCache();

  // Methods to trigger resize event.
  void TriggerResize();

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimensions of the video.
  blink::WebSize NaturalSize() const override;

  blink::WebSize VisibleRect() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  double CurrentTime() const override;

  // Internal states of loading and network.
  blink::WebMediaPlayer::NetworkState GetNetworkState() const override;
  blink::WebMediaPlayer::ReadyState GetReadyState() const override;

  blink::WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override;

  bool DidGetOpaqueResponseFromServiceWorker() const override;
  bool HasSingleSecurityOrigin() const override;
  bool DidPassCORSAccessCheck() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  size_t AudioDecodedByteCount() const override;
  size_t VideoDecodedByteCount() const override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameClosed() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override;
  void OnPlay() override;
  void OnPause() override;
  void OnSeekForward(double seconds) override;
  void OnSeekBackward(double seconds) override;
  void OnVolumeMultiplierUpdate(double multiplier) override;
  void OnBecamePersistentVideo(bool value) override;
  void OnPictureInPictureModeEnded() override;

  bool CopyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface* gl,
      unsigned target,
      unsigned int texture,
      unsigned internal_format,
      unsigned format,
      unsigned type,
      int level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata) override;

  bool TexImageImpl(TexImageFunctionID functionID,
                    unsigned target,
                    gpu::gles2::GLES2Interface* gl,
                    unsigned int texture,
                    int level,
                    int internalformat,
                    unsigned format,
                    unsigned type,
                    int xoffset,
                    int yoffset,
                    int zoffset,
                    bool flip_y,
                    bool premultiply_alpha) override;

  // blink::WebMediaStreamObserver implementation
  void TrackAdded(const blink::WebMediaStreamTrack& track) override;
  void TrackRemoved(const blink::WebMediaStreamTrack& track) override;
  void ActiveStateChanged(bool is_active) override;

 private:
  friend class WebMediaPlayerMSTest;

#if defined(OS_WIN)
  static const gfx::Size kUseGpuMemoryBufferVideoFramesMinResolution;
#endif  // defined(OS_WIN)

  void OnFirstFrameReceived(media::VideoRotation video_rotation,
                            bool is_opaque);
  void OnOpacityChanged(bool is_opaque);
  void OnRotationChanged(media::VideoRotation video_rotation, bool is_opaque);

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

  // To be run when tracks are added or removed.
  void Reload();
  void ReloadVideo();
  void ReloadAudio();

  // Helper method used for testing.
  void SetGpuMemoryBufferVideoForTesting(
      media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool);

  blink::WebLocalFrame* const frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  const blink::WebTimeRanges buffered_;

  blink::WebMediaPlayerClient* const client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  //
  // NOTE: HTMLMediaElement is a Blink::PausableObject, and will receive a
  // call to contextDestroyed() when Blink::Document::shutdown() is called.
  // Document::shutdown() is called before the frame detaches (and before the
  // frame is destroyed). RenderFrameImpl owns of |delegate_|, and is guaranteed
  // to outlive |this|. It is therefore safe use a raw pointer directly.
  media::WebMediaPlayerDelegate* delegate_;
  int delegate_id_;

  // Inner class used for transfering frames on compositor thread to
  // |compositor_|.
  class FrameDeliverer;
  std::unique_ptr<FrameDeliverer> frame_deliverer_;

  scoped_refptr<MediaStreamVideoRenderer> video_frame_provider_;  // Weak

  scoped_refptr<cc::VideoLayer> video_layer_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_;  // Weak
  media::PaintCanvasVideoRenderer video_renderer_;

  bool paused_;
  media::VideoRotation video_rotation_;

  std::unique_ptr<media::MediaLog> media_log_;

  std::unique_ptr<MediaStreamRendererFactory> renderer_factory_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::TaskRunner> worker_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<WebMediaPlayerMSCompositor> compositor_;

  const std::string initial_audio_output_device_id_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  // True if playback should be started upon the next call to OnShown(). Only
  // used on Android.
  bool should_play_upon_shown_;
  blink::WebMediaStream web_stream_;
  // IDs of the tracks currently played.
  blink::WebString current_video_track_id_;
  blink::WebString current_audio_track_id_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_WEBMEDIAPLAYER_MS_H_
