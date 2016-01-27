// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/pipeline.h"
#include "media/base/renderer_factory.h"
#include "media/base/text_track.h"
#include "media/blink/buffered_data_source.h"
#include "media/blink/buffered_data_source_host_impl.h"
#include "media/blink/encrypted_media_player_support.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/multibuffer_data_source.h"
#include "media/blink/video_frame_compositor.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)  // WMPI_CAST
// Delete this file when WMPI_CAST is no longer needed.
#include "media/blink/webmediaplayer_cast_android.h"
#endif

namespace blink {
class WebGraphicsContext3D;
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace media {

class AudioHardwareConfig;
class ChunkDemuxer;
class GpuVideoAcceleratorFactories;
class MediaLog;
class UrlIndex;
class VideoFrameCompositor;
class WebAudioSourceProviderImpl;
class WebMediaPlayerDelegate;
class WebTextTrackImpl;

// The canonical implementation of blink::WebMediaPlayer that's backed by
// Pipeline. Handles normal resource loading, Media Source, and
// Encrypted Media.
class MEDIA_BLINK_EXPORT WebMediaPlayerImpl
    : public NON_EXPORTED_BASE(blink::WebMediaPlayer),
      public NON_EXPORTED_BASE(WebMediaPlayerDelegate::Observer),
      public base::SupportsWeakPtr<WebMediaPlayerImpl> {
 public:
  // Constructs a WebMediaPlayer implementation using Chromium's media stack.
  // |delegate| may be null. |renderer_factory| must not be null.
  WebMediaPlayerImpl(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      base::WeakPtr<WebMediaPlayerDelegate> delegate,
      scoped_ptr<RendererFactory> renderer_factory,
      CdmFactory* cdm_factory,
      linked_ptr<UrlIndex> url_index,
      const WebMediaPlayerParams& params);
  ~WebMediaPlayerImpl() override;

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
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebSetSinkIdCallbacks* web_callback) override;
  void setPreload(blink::WebMediaPlayer::Preload preload) override;
  void setBufferingStrategy(
      blink::WebMediaPlayer::BufferingStrategy buffering_strategy) override;
  blink::WebTimeRanges buffered() const override;
  blink::WebTimeRanges seekable() const override;

  // paint() the current video frame into |canvas|. This is used to support
  // various APIs and functionalities, including but not limited to: <canvas>,
  // WebGL texImage2D, ImageBitmap, printing and capturing capabilities.
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
  virtual double timelineOffset() const;
  double currentTime() const override;

  // Internal states of loading and network.
  // TODO(hclam): Ask the pipeline about the state rather than having reading
  // them from members which would cause race conditions.
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

  blink::WebAudioSourceProvider* audioSourceProvider() override;

  MediaKeyException generateKeyRequest(
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length) override;

  MediaKeyException addKey(const blink::WebString& key_system,
                           const unsigned char* key,
                           unsigned key_length,
                           const unsigned char* init_data,
                           unsigned init_data_length,
                           const blink::WebString& session_id) override;

  MediaKeyException cancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id) override;

  void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) override;

  void OnPipelineSeeked(bool time_changed, PipelineStatus status);
  void OnPipelineSuspended(PipelineStatus status);
  void OnPipelineEnded();
  void OnPipelineError(PipelineStatus error);
  void OnPipelineMetadata(PipelineMetadata metadata);
  void OnPipelineBufferingStateChanged(BufferingState buffering_state);
  void OnDemuxerOpened();
  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb);

  // WebMediaPlayerDelegate::Observer implementation.
  void OnHidden() override;
  void OnShown() override;
  void OnPlay() override;
  void OnPause() override;
  void OnVolumeMultiplierUpdate(double multiplier) override;

#if defined(OS_ANDROID)  // WMPI_CAST
  bool isRemote() const override;
  void requestRemotePlayback() override;
  void requestRemotePlaybackControl() override;

  void SetMediaPlayerManager(
      RendererMediaPlayerManagerInterface* media_player_manager);
  void OnRemotePlaybackEnded();
  void OnDisconnectedFromRemoteDevice(double t);
  void SuspendForRemote();
  void DisplayCastFrameAfterSuspend(const scoped_refptr<VideoFrame>& new_frame,
                                    PipelineStatus status);
  gfx::Size GetCanvasSize() const;
  void SetDeviceScaleFactor(float scale_factor);
#endif

 private:
  // Ask for the pipeline to be suspended, will call Suspend() when ready.
  // (Possibly immediately.)
  void ScheduleSuspend();

  // Initiate suspending the pipeline.
  void Suspend();

  // Ask for the pipeline to be resumed, will call Resume() when ready.
  // (Possibly immediately.)
  void ScheduleResume();

  // Initiate resuming the pipeline.
  void Resume();

  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type,
              const blink::WebURL& url,
              CORSMode cors_mode);

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Creates a Renderer via the |renderer_factory_|.
  scoped_ptr<Renderer> CreateRenderer();

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Gets the duration value reported by the pipeline.
  double GetPipelineDuration() const;

  // Callbacks from |pipeline_| that are forwarded to |client_|.
  void OnDurationChanged();
  void OnNaturalSizeChanged(gfx::Size size);
  void OnOpacityChanged(bool opaque);

  // Called by VideoRendererImpl on its internal thread with the new frame to be
  // painted.
  void FrameReady(const scoped_refptr<VideoFrame>& frame);

  // Returns the current video frame from |compositor_|. Blocks until the
  // compositor can return the frame.
  scoped_refptr<VideoFrame> GetCurrentFrameFromCompositor();

  // Called when the demuxer encounters encrypted streams.
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);

  // Called when a decoder detects that the key needed to decrypt the stream
  // is not available.
  void OnWaitingForDecryptionKey();

  // Sets |cdm_context| on the pipeline and fires |cdm_attached_cb| when done.
  // Parameter order is reversed for easy binding.
  void SetCdm(const CdmAttachedCB& cdm_attached_cb, CdmContext* cdm_context);

  // Called when a CDM has been attached to the |pipeline_|.
  void OnCdmAttached(bool success);

  // Updates |paused_time_| to the current media time with consideration for the
  // |ended_| state by clamping current time to duration upon |ended_|.
  void UpdatePausedTime();

  // Notifies |delegate_| that playback has started or was paused; also starts
  // or stops the memory usage reporting timer respectively.
  void NotifyPlaybackStarted();
  void NotifyPlaybackPaused();

  // Called at low frequency to tell external observers how much memory we're
  // using for video playback.  Called by |memory_usage_reporting_timer_|.
  // Memory usage reporting is done in two steps, because |demuxer_| must be
  // accessed on the media thread.
  void ReportMemoryUsage();
  void FinishMemoryUsageReport(int64_t demuxer_memory_usage);

  blink::WebLocalFrame* frame_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // Preload state for when |data_source_| is created after setPreload().
  BufferedDataSource::Preload preload_;

  // Buffering strategy for when |data_source_| is created after
  // setBufferingStrategy().
  BufferedDataSource::BufferingStrategy buffering_strategy_;

  // Task runner for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;
  scoped_refptr<MediaLog> media_log_;
  Pipeline pipeline_;

  // The LoadType passed in the |load_type| parameter of the load() call.
  LoadType load_type_;

  // Cache of metadata for answering hasAudio(), hasVideo(), and naturalSize().
  PipelineMetadata pipeline_metadata_;

  // Whether the video is known to be opaque or not.
  bool opaque_;

  // Playback state.
  //
  // TODO(scherkus): we have these because Pipeline favours the simplicity of a
  // single "playback rate" over worrying about paused/stopped etc...  It forces
  // all clients to manage the pause+playback rate externally, but is that
  // really a bad thing?
  //
  // TODO(scherkus): since SetPlaybackRate(0) is asynchronous and we don't want
  // to hang the render thread during pause(), we record the time at the same
  // time we pause and then return that value in currentTime().  Otherwise our
  // clock can creep forward a little bit while the asynchronous
  // SetPlaybackRate(0) is being executed.
  double playback_rate_;
  bool paused_;
  base::TimeDelta paused_time_;
  bool seeking_;

  // Set when seeking (|seeking_| is true) or resuming.
  base::TimeDelta seek_time_;

  // Set when a suspend is required but another suspend or seek is in progress.
  bool pending_suspend_;

  // Set when suspending immediately after a seek. The time change will happen
  // after Resume().
  bool pending_time_change_;

  // Set when a resume is required but suspending is in progress.
  bool pending_resume_;

  // Set for the entire period between suspend starting and resume completing.
  bool suspending_;

  // Set while suspending to detect double-suspend.
  bool suspended_;

  // Set while resuming to detect double-resume.
  bool resuming_;

  // TODO(scherkus): Replace with an explicit ended signal to HTMLMediaElement,
  // see http://crbug.com/409280
  bool ended_;

  // Indicates that a seek is queued after the current seek completes or, if the
  // pipeline is suspended, after it resumes. Only the last queued seek will
  // have any effect.
  bool pending_seek_;

  // |pending_seek_time_| is meaningless when |pending_seek_| is false.
  base::TimeDelta pending_seek_time_;

  // Tracks whether to issue time changed notifications during buffering state
  // changes.
  bool should_notify_time_changed_;

  blink::WebMediaPlayerClient* client_;
  blink::WebMediaPlayerEncryptedMediaClient* encrypted_client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  base::WeakPtr<WebMediaPlayerDelegate> delegate_;
  int delegate_id_;

  WebMediaPlayerParams::DeferLoadCB defer_load_cb_;
  WebMediaPlayerParams::Context3DCB context_3d_cb_;

  // Members for notifying upstream clients about internal memory usage.  The
  // |adjust_allocated_memory_cb_| must only be called on |main_task_runner_|.
  base::RepeatingTimer memory_usage_reporting_timer_;
  WebMediaPlayerParams::AdjustAllocatedMemoryCB adjust_allocated_memory_cb_;
  int64_t last_reported_memory_usage_;

  // Routes audio playback to either AudioRendererSink or WebAudio.
  scoped_refptr<WebAudioSourceProviderImpl> audio_source_provider_;

  bool supports_save_;

  // These two are mutually exclusive:
  //   |data_source_| is used for regular resource loads.
  //   |chunk_demuxer_| is used for Media Source resource loads.
  //
  // |demuxer_| will contain the appropriate demuxer based on which resource
  // load strategy we're using.
  scoped_ptr<BufferedDataSourceInterface> data_source_;
  scoped_ptr<Demuxer> demuxer_;
  ChunkDemuxer* chunk_demuxer_;

  BufferedDataSourceHostImpl buffered_data_source_host_;
  linked_ptr<UrlIndex> url_index_;

  // Video rendering members.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  VideoFrameCompositor* compositor_;  // Deleted on |compositor_task_runner_|.
  SkCanvasVideoRenderer skcanvas_video_renderer_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  EncryptedMediaPlayerSupport encrypted_media_support_;

  scoped_ptr<blink::WebContentDecryptionModuleResult> set_cdm_result_;

  // Whether a CDM has been successfully attached.
  bool is_cdm_attached_;

#if defined(OS_ANDROID)  // WMPI_CAST
  WebMediaPlayerCast cast_impl_;
#endif

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  scoped_ptr<RendererFactory> renderer_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_IMPL_H_
