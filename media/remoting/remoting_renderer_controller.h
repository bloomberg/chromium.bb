// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_
#define MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/base/media_observer.h"
#include "media/remoting/remoting_interstitial_ui.h"
#include "media/remoting/remoting_source_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

namespace remoting {
class RpcBroker;
}

// This class:
// 1) Implements the RemotingSourceImpl::Client;
// 2) Monitors player events as a MediaObserver;
// 3) May trigger the switch of the media renderer between local playback
// and remoting.
class RemotingRendererController final : public RemotingSourceImpl::Client,
                                         public MediaObserver {
 public:
  explicit RemotingRendererController(
      scoped_refptr<RemotingSourceImpl> remoting_source);
  ~RemotingRendererController() override;

  // RemotingSourceImpl::Client implemenations.
  void OnStarted(bool success) override;
  void OnSessionStateChanged() override;

  // MediaObserver implementations.
  void OnEnteredFullscreen() override;
  void OnExitedFullscreen() override;
  void OnBecameDominantVisibleContent(bool is_dominant) override;
  void OnSetCdm(CdmContext* cdm_context) override;
  void OnMetadataChanged(const PipelineMetadata& metadata) override;
  void OnRemotePlaybackDisabled(bool disabled) override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnSetPoster(const GURL& poster) override;

  void SetSwitchRendererCallback(const base::Closure& cb);
  void SetRemoteSinkAvailableChangedCallback(
      const base::Callback<void(bool)>& cb);

  using ShowInterstitialCallback =
      base::Callback<void(const base::Optional<SkBitmap>&,
                          const gfx::Size&,
                          RemotingInterstitialType type)>;
  // Called by RemoteRendererImpl constructor to set the callback to draw and
  // show remoting interstial.
  void SetShowInterstitialCallback(const ShowInterstitialCallback& cb);
  using DownloadPosterCallback =
      base::Callback<void(const GURL&,
                          const base::Callback<void(const SkBitmap&)>&)>;
  // Set the callback to download poster image.
  void SetDownloadPosterCallback(const DownloadPosterCallback& cb);

  base::WeakPtr<RemotingRendererController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used by RemotingRendererFactory to query whether to create Media Remoting
  // Renderer.
  bool remote_rendering_started() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remote_rendering_started_;
  }

  void StartDataPipe(
      std::unique_ptr<mojo::DataPipe> audio_data_pipe,
      std::unique_ptr<mojo::DataPipe> video_data_pipe,
      const RemotingSourceImpl::DataPipeStartCallback& done_callback);

  // Used by RemotingRendererImpl to query the session state.
  RemotingSourceImpl* remoting_source() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remoting_source_.get();
  }

  base::WeakPtr<remoting::RpcBroker> GetRpcBroker() const;

  // Called by RemoteRendererImpl when irregular playback is detected, which
  // indicates either insufficient network bandwidth or the receiver cannot
  // handle the data volume of the content (e.g., too high resolution and/or
  // frame rate).
  void OnIrregularPlaybackDetected();

 private:
  bool has_audio() const {
    return pipeline_metadata_.has_audio &&
           pipeline_metadata_.audio_decoder_config.IsValidConfig();
  }

  bool has_video() const {
    return pipeline_metadata_.has_video &&
           pipeline_metadata_.video_decoder_config.IsValidConfig();
  }

  bool IsVideoCodecSupported();
  bool IsAudioCodecSupported();
  bool IsRemoteSinkAvailable();

  // Helper to decide whether to enter or leave Remoting mode.
  bool ShouldBeRemoting();

  // Determines whether to enter or leave Remoting mode and switches if
  // necessary.
  void UpdateAndMaybeSwitch();

  // Called to download the poster image. Called when:
  // 1. Poster URL changes.
  // 2. ShowInterstitialCallback is set.
  // 3. DownloadPosterCallback is set.
  void DownloadPosterImage();

  // Called when poster image is downloaded.
  void OnPosterImageDownloaded(const GURL& download_url, const SkBitmap& image);

  // Update remoting interstitial with |image|. When |image| is not set,
  // interstitial will be drawn on previously downloaded poster image (in
  // RemoteRendererImpl) or black background if none was downloaded before.
  // Call this when:
  // 1. SetShowInterstitialCallback() is called (RemoteRendererImpl is created).
  // 2. The remoting session is shut down (to update the status message in the
  //    interstitial).
  // 3. The size of the canvas is changed (to update the background image and
  //    the position of the status message).
  // 4. Poster image is downloaded (to update the background image).
  void UpdateInterstitial(const base::Optional<SkBitmap>& image);

  // Indicates whether this media element is in full screen.
  bool is_fullscreen_ = false;

  // Indicates whether remoting is started.
  bool remote_rendering_started_ = false;

  // Indicates whether audio or video is encrypted.
  bool is_encrypted_ = false;

  // Indicates whether remote playback is currently disabled. This starts out as
  // true, and should be updated at least once via a call to
  // OnRemotePlaybackDisabled() at some point in the future. A web page
  // typically sets/removes the disableRemotePlayback attribute on a
  // HTMLMediaElement to disable/enable remoting of its content. Please see the
  // Remote Playback API spec for more details:
  // https://w3c.github.io/remote-playback
  bool is_remote_playback_disabled_ = true;

  // Indicates whether video is the dominant visible content in the tab.
  bool is_dominant_content_ = false;

  // Indicates whether video is paused.
  bool is_paused_ = true;

  // Indicates whether OnIrregularPlaybackDetected() has been called.
  bool irregular_playback_detected_ = false;

  // The callback to switch the media renderer.
  base::Closure switch_renderer_cb_;

  // Called when remoting sink availability is changed.
  base::Callback<void(bool)> sink_available_changed_cb_;

  // This is initially the RemotingSourceImpl passed to the ctor, and might be
  // replaced with a different instance later if OnSetCdm() is called.
  scoped_refptr<RemotingSourceImpl> remoting_source_;

  // This is used to check all the methods are called on the current thread in
  // debug builds.
  base::ThreadChecker thread_checker_;

  // Current pipeline metadata.
  PipelineMetadata pipeline_metadata_;

  // The callback to show remoting interstitial. It is set when entering the
  // remoting mode (RemotingRendererImpl is constructed) by calling
  // SetShowInterstitialCallback(), and is reset when leaving the remoting mode.
  ShowInterstitialCallback show_interstitial_cb_;

  // Current poster URL, whose image will feed into the local UI.
  GURL poster_url_;

  // The callback to download the poster image. Called when |poster_url_|
  // changes during a remoting session or the show interstial callback is set.
  // OnPosterImageDownloaded() will be called when download completes.
  DownloadPosterCallback download_poster_cb_;

  base::WeakPtrFactory<RemotingRendererController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemotingRendererController);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_RENDERER_CONTROLLER_H_
