// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_tracks.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace media {

MediaTracks::MediaTracks() {}

MediaTracks::~MediaTracks() {}

void MediaTracks::AddAudioTrack(const AudioDecoderConfig& config,
                                const std::string& id,
                                const std::string& kind,
                                const std::string& label,
                                const std::string& language) {
  DCHECK(config.IsValidConfig());
  CHECK(audio_configs_.find(id) == audio_configs_.end());
  std::unique_ptr<MediaTrack> track = base::WrapUnique(
      new MediaTrack(MediaTrack::Audio, id, kind, label, language));
  tracks_.push_back(std::move(track));
  audio_configs_[id] = config;
}

void MediaTracks::AddVideoTrack(const VideoDecoderConfig& config,
                                const std::string& id,
                                const std::string& kind,
                                const std::string& label,
                                const std::string& language) {
  DCHECK(config.IsValidConfig());
  CHECK(video_configs_.find(id) == video_configs_.end());
  std::unique_ptr<MediaTrack> track = base::WrapUnique(
      new MediaTrack(MediaTrack::Video, id, kind, label, language));
  tracks_.push_back(std::move(track));
  video_configs_[id] = config;
}

const AudioDecoderConfig& MediaTracks::getAudioConfig(
    const std::string& id) const {
  auto it = audio_configs_.find(id);
  if (it != audio_configs_.end())
    return it->second;
  static AudioDecoderConfig invalidConfig;
  return invalidConfig;
}

const VideoDecoderConfig& MediaTracks::getVideoConfig(
    const std::string& id) const {
  auto it = video_configs_.find(id);
  if (it != video_configs_.end())
    return it->second;
  static VideoDecoderConfig invalidConfig;
  return invalidConfig;
}

const AudioDecoderConfig& MediaTracks::getFirstAudioConfig() const {
  for (const auto& track : tracks()) {
    if (track->type() == MediaTrack::Audio) {
      return getAudioConfig(track->id());
    }
  }
  static AudioDecoderConfig invalidConfig;
  return invalidConfig;
}

const VideoDecoderConfig& MediaTracks::getFirstVideoConfig() const {
  for (const auto& track : tracks()) {
    if (track->type() == MediaTrack::Video) {
      return getVideoConfig(track->id());
    }
  }
  static VideoDecoderConfig invalidConfig;
  return invalidConfig;
}

}  // namespace media
