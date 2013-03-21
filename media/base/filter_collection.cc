// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"

#include "base/logging.h"
#include "media/base/audio_renderer.h"
#include "media/base/demuxer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_renderer.h"

namespace media {

FilterCollection::FilterCollection() {}

FilterCollection::~FilterCollection() {}

void FilterCollection::SetDemuxer(const scoped_refptr<Demuxer>& demuxer) {
  demuxer_ = demuxer;
}

const scoped_refptr<Demuxer>& FilterCollection::GetDemuxer() {
  return demuxer_;
}

void FilterCollection::SetAudioRenderer(
    scoped_ptr<AudioRenderer> audio_renderer) {
  audio_renderer_ = audio_renderer.Pass();
}

scoped_ptr<AudioRenderer> FilterCollection::GetAudioRenderer() {
  return audio_renderer_.Pass();
}

void FilterCollection::SetVideoRenderer(
    scoped_ptr<VideoRenderer> video_renderer) {
  video_renderer_ = video_renderer.Pass();
}

scoped_ptr<VideoRenderer> FilterCollection::GetVideoRenderer() {
  return video_renderer_.Pass();
}

void FilterCollection::Clear() {
  video_decoders_.clear();
  audio_renderer_.reset();
  video_renderer_.reset();
}

FilterCollection::VideoDecoderList* FilterCollection::GetVideoDecoders() {
  return &video_decoders_;
}

}  // namespace media
