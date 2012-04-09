// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_wtl/movie.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "media/audio/audio_manager.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "media/base/pipeline.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/video_renderer_base.h"

namespace media {

Movie::Movie()
    : audio_manager_(AudioManager::Create()),
      enable_audio_(false),
      enable_draw_(true),
      enable_dump_yuv_file_(false),
      enable_pause_(false),
      max_threads_(0),
      play_rate_(1.0f),
      movie_dib_(NULL),
      movie_hwnd_(0) {
}

Movie::~Movie() {
}

Movie* Movie::GetInstance() {
  return Singleton<Movie>::get();
}

bool Movie::IsOpen() {
  return pipeline_ != NULL;
}

void Movie::SetFrameBuffer(HBITMAP hbmp, HWND hwnd) {
  movie_dib_ = hbmp;
  movie_hwnd_ = hwnd;
}

bool Movie::Open(const wchar_t* url, VideoRendererBase* video_renderer) {
  // Close previous movie.
  if (pipeline_) {
    Close();
  }

  message_loop_factory_.reset(new media::MessageLoopFactory());

  MessageLoop* pipeline_loop =
      message_loop_factory_->GetMessageLoop("PipelineThread");
  pipeline_ = new Pipeline(pipeline_loop, new media::MediaLog());

  // Open the file.
  std::string url_utf8 = WideToUTF8(string16(url));
  scoped_refptr<FileDataSource> data_source = new FileDataSource();
  if (data_source->Initialize(url_utf8) != PIPELINE_OK) {
    return false;
  }

  // Create filter collection.
  scoped_ptr<FilterCollection> collection(new FilterCollection());
  collection->SetDemuxer(new FFmpegDemuxer(
      pipeline_loop, data_source, true));
  collection->AddAudioDecoder(new FFmpegAudioDecoder(
      base::Bind(&MessageLoopFactory::GetMessageLoop,
                 base::Unretained(message_loop_factory_.get()),
                 "AudioDecoderThread")));
  collection->AddVideoDecoder(new FFmpegVideoDecoder(
      base::Bind(&MessageLoopFactory::GetMessageLoop,
                 base::Unretained(message_loop_factory_.get()),
                 "VideoDecoderThread")));

  // TODO(vrk): Re-enabled audio. (crbug.com/112159)
  collection->AddAudioRenderer(
      new media::AudioRendererImpl(new media::NullAudioSink()));
  collection->AddVideoRenderer(video_renderer);

  // Create and start our pipeline.
  media::PipelineStatusNotification note;
  pipeline_->Start(
      collection.Pass(),
      media::PipelineStatusCB(),
      media::PipelineStatusCB(),
      media::NetworkEventCB(),
      note.Callback());

  // Wait until the pipeline is fully initialized.
  note.Wait();
  if (note.status() != PIPELINE_OK)
    return false;
  pipeline_->SetPlaybackRate(play_rate_);
  return true;
}

void Movie::Play(float rate) {
  // Begin playback.
  if (pipeline_)
    pipeline_->SetPlaybackRate(enable_pause_ ? 0.0f : rate);
  if (rate > 0.0f)
    play_rate_ = rate;
}

// Get playback rate.
float Movie::GetPlayRate() {
  return play_rate_;
}

// Get movie duration in seconds.
float Movie::GetDuration() {
  float duration = 0.f;
  if (pipeline_)
    duration = (pipeline_->GetMediaDuration()).InMicroseconds() / 1000000.0f;
  return duration;
}

// Get current movie position in seconds.
float Movie::GetPosition() {
  float position = 0.f;
  if (pipeline_)
    position = (pipeline_->GetCurrentTime()).InMicroseconds() / 1000000.0f;
  return position;
}

// Set current movie position in seconds.
void Movie::SetPosition(float position) {
  int64 us = static_cast<int64>(position * 1000000);
  base::TimeDelta time = base::TimeDelta::FromMicroseconds(us);
  if (pipeline_)
    pipeline_->Seek(time, media::PipelineStatusCB());
}


// Set playback pause.
void Movie::SetPause(bool pause) {
  enable_pause_ = pause;
  Play(play_rate_);
}

// Get playback pause state.
bool Movie::GetPause() {
  return enable_pause_;
}

void Movie::SetAudioEnable(bool enable_audio) {
}

bool Movie::GetAudioEnable() {
  return enable_audio_;
}

void Movie::SetDrawEnable(bool enable_draw) {
  enable_draw_ = enable_draw;
}

bool Movie::GetDrawEnable() {
  return enable_draw_;
}

void Movie::SetDumpYuvFileEnable(bool enable_dump_yuv_file) {
  enable_dump_yuv_file_ = enable_dump_yuv_file;
}

bool Movie::GetDumpYuvFileEnable() {
  return enable_dump_yuv_file_;
}

// Teardown.
void Movie::Close() {
  if (pipeline_) {
    pipeline_->Stop(base::Closure());
    pipeline_ = NULL;
  }

  message_loop_factory_.reset();
}

}  // namespace media
