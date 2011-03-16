// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_wtl/movie.h"

#include "base/singleton.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source_factory.h"
#include "media/filters/null_audio_renderer.h"
#include "media/tools/player_wtl/wtl_renderer.h"

using media::AudioRendererImpl;
using media::FFmpegAudioDecoder;
using media::FFmpegDemuxerFactory;
using media::FFmpegVideoDecoder;
using media::FileDataSourceFactory;
using media::FilterCollection;
using media::PipelineImpl;

namespace media {

Movie::Movie()
    : enable_audio_(true),
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

bool Movie::Open(const wchar_t* url, WtlVideoRenderer* video_renderer) {
  // Close previous movie.
  if (pipeline_) {
    Close();
  }

  message_loop_factory_.reset(new media::MessageLoopFactoryImpl());

  MessageLoop* pipeline_loop =
      message_loop_factory_->GetMessageLoop("PipelineThread");
  pipeline_ = new PipelineImpl(pipeline_loop);

  // Create filter collection.
  scoped_ptr<FilterCollection> collection(new FilterCollection());
  collection->SetDemuxerFactory(new FFmpegDemuxerFactory(
      new FileDataSourceFactory(), pipeline_loop));
  collection->AddAudioDecoder(new FFmpegAudioDecoder(
      message_loop_factory_->GetMessageLoop("AudioDecoderThread")));
  collection->AddVideoDecoder(new FFmpegVideoDecoder(
      message_loop_factory_->GetMessageLoop("VideoDecoderThread"), NULL));

  if (enable_audio_) {
    collection->AddAudioRenderer(new AudioRendererImpl());
  } else {
    collection->AddAudioRenderer(new media::NullAudioRenderer());
  }
  collection->AddVideoRenderer(video_renderer);

  // Create and start our pipeline.
  media::PipelineStatusNotification note;
  pipeline_->Start(collection.release(), WideToUTF8(std::wstring(url)),
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
    pipeline_->Seek(time, NULL);
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
  enable_audio_ = enable_audio;
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
    pipeline_->Stop(NULL);
    pipeline_ = NULL;
  }

  message_loop_factory_.reset();
}

}  // namespace media
