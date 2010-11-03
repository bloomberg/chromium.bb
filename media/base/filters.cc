// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"
#include "base/message_loop.h"

namespace media {

MediaFilter::MediaFilter() : host_(NULL), message_loop_(NULL) {}

const char* MediaFilter::major_mime_type() const {
  return "";
}

void MediaFilter::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

FilterHost* MediaFilter::host() {
  return host_;
}

void MediaFilter::set_message_loop(MessageLoop* message_loop) {
  DCHECK(message_loop);
  DCHECK(!message_loop_);
  message_loop_ = message_loop;
}

MessageLoop* MediaFilter::message_loop() {
  return message_loop_;
}

void MediaFilter::Play(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void MediaFilter::Pause(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void MediaFilter::Flush(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void MediaFilter::Stop(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void MediaFilter::SetPlaybackRate(float playback_rate) {}

void MediaFilter::Seek(base::TimeDelta time, FilterCallback* callback) {
  scoped_ptr<FilterCallback> seek_callback(callback);
  if (seek_callback.get()) {
    seek_callback->Run();
  }
}

void MediaFilter::OnAudioRendererDisabled() {
}

MediaFilter::~MediaFilter() {}

bool DataSource::IsUrlSupported(const std::string& url) {
  return true;
}

FilterType DataSource::filter_type() const {
  return static_filter_type();
}

FilterType Demuxer::filter_type() const {
  return static_filter_type();
}

FilterType AudioDecoder::filter_type() const {
  return static_filter_type();
}

const char* AudioDecoder::major_mime_type() const {
  return mime_type::kMajorTypeAudio;
}

FilterType AudioRenderer::filter_type() const {
  return static_filter_type();
}

const char* AudioRenderer::major_mime_type() const {
  return mime_type::kMajorTypeAudio;
}

FilterType VideoDecoder::filter_type() const {
  return static_filter_type();
}

const char* VideoDecoder::major_mime_type() const {
  return mime_type::kMajorTypeVideo;
}

FilterType VideoRenderer::filter_type() const {
  return static_filter_type();
}

const char* VideoRenderer::major_mime_type() const {
  return mime_type::kMajorTypeVideo;
}

DemuxerStream::~DemuxerStream() {}

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

}  // namespace media
