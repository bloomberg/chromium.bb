// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"
#include "base/message_loop.h"

namespace media {

Filter::Filter() : host_(NULL), message_loop_(NULL) {}

Filter::~Filter() {}

const char* Filter::major_mime_type() const {
  return "";
}

void Filter::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

FilterHost* Filter::host() {
  return host_;
}

bool Filter::requires_message_loop() const {
  return false;
}

const char* Filter::message_loop_name() const {
  return "FilterThread";
}

void Filter::set_message_loop(MessageLoop* message_loop) {
  DCHECK(message_loop);
  DCHECK(!message_loop_);
  message_loop_ = message_loop;
}

MessageLoop* Filter::message_loop() {
  return message_loop_;
}

void Filter::Play(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void Filter::Pause(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void Filter::Flush(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void Filter::Stop(FilterCallback* callback) {
  DCHECK(callback);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void Filter::SetPlaybackRate(float playback_rate) {}

void Filter::Seek(base::TimeDelta time, FilterCallback* callback) {
  scoped_ptr<FilterCallback> seek_callback(callback);
  if (seek_callback.get()) {
    seek_callback->Run();
  }
}

void Filter::OnAudioRendererDisabled() {
}

bool DataSource::IsUrlSupported(const std::string& url) {
  return true;
}

bool Demuxer::requires_message_loop() const {
  return true;
}

const char* Demuxer::message_loop_name() const {
  return "DemuxerThread";
}

const char* AudioDecoder::major_mime_type() const {
  return mime_type::kMajorTypeAudio;
}

bool AudioDecoder::requires_message_loop() const {
  return true;
}

const char* AudioDecoder::message_loop_name() const {
  return "AudioDecoderThread";
}

const char* AudioRenderer::major_mime_type() const {
  return mime_type::kMajorTypeAudio;
}

const char* VideoDecoder::major_mime_type() const {
  return mime_type::kMajorTypeVideo;
}

bool VideoDecoder::requires_message_loop() const {
  return true;
}

const char* VideoDecoder::message_loop_name() const {
  return "VideoDecoderThread";
}

const char* VideoRenderer::major_mime_type() const {
  return mime_type::kMajorTypeVideo;
}

void* DemuxerStream::QueryInterface(const char* interface_id) {
  return NULL;
}

DemuxerStream::~DemuxerStream() {}

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

}  // namespace media
