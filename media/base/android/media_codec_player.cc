// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_player.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"

#define RUN_ON_MEDIA_THREAD(METHOD, ...)                                      \
  do {                                                                        \
    if (!GetMediaTaskRunner()->BelongsToCurrentThread()) {                    \
      GetMediaTaskRunner()->PostTask(                                         \
          FROM_HERE,                                                          \
          base::Bind(&MediaCodecPlayer:: METHOD, weak_this_, ##__VA_ARGS__)); \
      return;                                                                 \
    }                                                                         \
  } while(0)


namespace media {

class MediaThread : public base::Thread {
 public:
  MediaThread() : base::Thread("BrowserMediaThread") {
    Start();
  }
};

// Create media thread
base::LazyInstance<MediaThread>::Leaky
    g_media_thread = LAZY_INSTANCE_INITIALIZER;


scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner() {
  return g_media_thread.Pointer()->task_runner();
}

// MediaCodecPlayer implementation.

MediaCodecPlayer::MediaCodecPlayer(
    int player_id,
    MediaPlayerManager* manager,
    const RequestMediaResourcesCB& request_media_resources_cb,
    scoped_ptr<DemuxerAndroid> demuxer,
    const GURL& frame_url)
    : MediaPlayerAndroid(player_id,
                         manager,
                         request_media_resources_cb,
                         frame_url),
      ui_task_runner_(base::MessageLoopProxy::current()),
      demuxer_(demuxer.Pass()),
      weak_factory_(this) {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "MediaCodecPlayer::MediaCodecPlayer: player_id:" << player_id;

  weak_this_ = weak_factory_.GetWeakPtr();

  // Finish initializaton on Media thread
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&MediaCodecPlayer::Initialize, weak_this_));
}

MediaCodecPlayer::~MediaCodecPlayer()
{
  // Media thread
  DVLOG(1) << "MediaCodecPlayer::~MediaCodecPlayer";
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
}

void MediaCodecPlayer::Initialize() {
  // Media thread
  DVLOG(1) << __FUNCTION__;
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  demuxer_->Initialize(this);
}

// MediaPlayerAndroid implementation.

void MediaCodecPlayer::DeleteOnCorrectThread() {
  // UI thread
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // The listener-related portion of the base class has to be
  // destroyed on UI thread.
  DestroyListenerOnUIThread();

  // Post deletion onto Media thread
  GetMediaTaskRunner()->DeleteSoon(FROM_HERE, this);
}

void MediaCodecPlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  RUN_ON_MEDIA_THREAD(SetVideoSurface, base::Passed(&surface));

  // Media thread
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::Start() {
  RUN_ON_MEDIA_THREAD(Start);

  // Media thread
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::Pause(bool is_media_related_action) {
  RUN_ON_MEDIA_THREAD(Pause, is_media_related_action);

  // Media thread
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::SeekTo(base::TimeDelta timestamp) {
  RUN_ON_MEDIA_THREAD(SeekTo, timestamp);

  // Media thread
  DVLOG(1) << __FUNCTION__ << " " << timestamp;

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::Release() {
  RUN_ON_MEDIA_THREAD(Release);

  // Media thread
  DVLOG(1) << __FUNCTION__;

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::SetVolume(double volume) {
  RUN_ON_MEDIA_THREAD(SetVolume, volume);

  // Media thread
  DVLOG(1) << __FUNCTION__ << " " << volume;

  NOTIMPLEMENTED();
}

int MediaCodecPlayer::GetVideoWidth() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return 320;
}

int MediaCodecPlayer::GetVideoHeight() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return 240;
}

base::TimeDelta MediaCodecPlayer::GetCurrentTime() {
  // UI thread, Media thread
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

base::TimeDelta MediaCodecPlayer::GetDuration() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return base::TimeDelta();
}

bool MediaCodecPlayer::IsPlaying() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanPause() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekForward() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::CanSeekBackward() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

bool MediaCodecPlayer::IsPlayerReady() {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return true;
}

void MediaCodecPlayer::SetCdm(BrowserCdm* cdm) {
  // UI thread
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

// Callbacks from Demuxer.

void MediaCodecPlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  // Media thread
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

void MediaCodecPlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  // Media thread
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void MediaCodecPlayer::OnDemuxerSeekDone(
    base::TimeDelta actual_browser_seek_time) {
  // Media thread
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void MediaCodecPlayer::OnDemuxerDurationChanged(
    base::TimeDelta duration) {
  // Media thread
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

}  // namespace media
