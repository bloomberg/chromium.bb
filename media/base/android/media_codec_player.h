// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_player_android.h"
#include "media/base/media_export.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

class BrowserCdm;

// Returns the task runner for the media thread
MEDIA_EXPORT scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();


// This class implements the media player using Android's MediaCodec.
// It differs from MediaSourcePlayer in that it removes most
// processing away from UI thread: it uses a dedicated Media thread
// to receive the data and to handle commands.
class MEDIA_EXPORT MediaCodecPlayer : public MediaPlayerAndroid,
                                      public DemuxerAndroidClient {
 public:
  // Constructs a player with the given ID and demuxer. |manager| must outlive
  // the lifetime of this object.
  MediaCodecPlayer(int player_id,
                   MediaPlayerManager* manager,
                   const RequestMediaResourcesCB& request_media_resources_cb,
                   scoped_ptr<DemuxerAndroid> demuxer,
                   const GURL& frame_url);
  ~MediaCodecPlayer() override;

  // MediaPlayerAndroid implementation.
  void DeleteOnCorrectThread() override;
  void SetVideoSurface(gfx::ScopedJavaSurface surface) override;
  void Start() override;
  void Pause(bool is_media_related_action) override;
  void SeekTo(base::TimeDelta timestamp) override;
  void Release() override;
  void SetVolume(double volume) override;
  int GetVideoWidth() override;
  int GetVideoHeight() override;
  base::TimeDelta GetCurrentTime() override;
  base::TimeDelta GetDuration() override;
  bool IsPlaying() override;
  bool CanPause() override;
  bool CanSeekForward() override;
  bool CanSeekBackward() override;
  bool IsPlayerReady() override;
  void SetCdm(BrowserCdm* cdm) override;

  // DemuxerAndroidClient implementation.
  void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) override;
  void OnDemuxerDataAvailable(const DemuxerData& params) override;
  void OnDemuxerSeekDone(base::TimeDelta actual_browser_seek_time) override;
  void OnDemuxerDurationChanged(base::TimeDelta duration) override;

  // Helper methods
  void Initialize();
  void DestroySelf();

 private:
  // Object for posting tasks on UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  scoped_ptr<DemuxerAndroid> demuxer_;

  base::WeakPtr<MediaCodecPlayer> weak_this_;
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaCodecPlayer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecPlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
