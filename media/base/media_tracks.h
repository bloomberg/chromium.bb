// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TRACKS_H_
#define MEDIA_BASE_MEDIA_TRACKS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/media_track.h"

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

class MEDIA_EXPORT MediaTracks {
 public:
  using MediaTracksCollection = std::vector<std::unique_ptr<MediaTrack>>;

  MediaTracks();
  ~MediaTracks();

  // Callers need to ensure that track id is unique.
  void AddAudioTrack(const AudioDecoderConfig& config,
                     const std::string& id,
                     const std::string& kind,
                     const std::string& label,
                     const std::string& language);
  // Callers need to ensure that track id is unique.
  void AddVideoTrack(const VideoDecoderConfig& config,
                     const std::string& id,
                     const std::string& kind,
                     const std::string& label,
                     const std::string& language);

  const MediaTracksCollection& tracks() const { return tracks_; }

  const AudioDecoderConfig& getAudioConfig(const std::string& id) const;
  const VideoDecoderConfig& getVideoConfig(const std::string& id) const;

  // TODO(servolk): These are temporary helpers useful until all code paths are
  // converted to properly handle multiple media tracks.
  const AudioDecoderConfig& getFirstAudioConfig() const;
  const VideoDecoderConfig& getFirstVideoConfig() const;

 private:
  MediaTracksCollection tracks_;
  std::map<std::string, AudioDecoderConfig> audio_configs_;
  std::map<std::string, VideoDecoderConfig> video_configs_;

  DISALLOW_COPY_AND_ASSIGN(MediaTracks);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TRACKS_H_
