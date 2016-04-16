// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_tracks.h"

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace media {

MediaTracks::MediaTracks() {}

MediaTracks::~MediaTracks() {}

const MediaTrack* MediaTracks::AddAudioTrack(const AudioDecoderConfig& config,
                                             const std::string& id,
                                             const std::string& kind,
                                             const std::string& label,
                                             const std::string& language) {
  DCHECK(config.IsValidConfig());
  CHECK(audio_configs_.find(id) == audio_configs_.end());
  scoped_ptr<MediaTrack> track = make_scoped_ptr(
      new MediaTrack(MediaTrack::Audio, id, kind, label, language));
  tracks_.push_back(std::move(track));
  audio_configs_[id] = config;
  return tracks_.back().get();
}

const MediaTrack* MediaTracks::AddVideoTrack(const VideoDecoderConfig& config,
                                             const std::string& id,
                                             const std::string& kind,
                                             const std::string& label,
                                             const std::string& language) {
  DCHECK(config.IsValidConfig());
  CHECK(video_configs_.find(id) == video_configs_.end());
  scoped_ptr<MediaTrack> track = make_scoped_ptr(
      new MediaTrack(MediaTrack::Video, id, kind, label, language));
  tracks_.push_back(std::move(track));
  video_configs_[id] = config;
  return tracks_.back().get();
}

void MediaTracks::SetDemuxerStreamForMediaTrack(const MediaTrack* track,
                                                const DemuxerStream* stream) {
  DCHECK(track_to_demux_stream_map_.find(track) ==
         track_to_demux_stream_map_.end());

  bool track_found = false;
  for (const auto& t : tracks_) {
    if (t.get() == track) {
      track_found = true;
      break;
    }
  }
  CHECK(track_found);

  track_to_demux_stream_map_[track] = stream;
}

MediaTracks::TrackIdToDemuxStreamMap MediaTracks::OnTrackIdsAssigned(
    const std::vector<unsigned>& track_ids) const {
  TrackIdToDemuxStreamMap result;
  CHECK_EQ(tracks().size(), track_ids.size());
  CHECK_EQ(track_to_demux_stream_map_.size(), tracks().size());
  for (size_t i = 0; i < track_ids.size(); ++i) {
    const MediaTrack* track = tracks()[i].get();
    DCHECK(track);
    const auto& it = track_to_demux_stream_map_.find(track);
    CHECK(it != track_to_demux_stream_map_.end());
    DVLOG(3) << "OnTrackIdsAssigned track_id=" << track_ids[i]
             << " DemuxerStream=" << it->second;
    result[track_ids[i]] = it->second;
  }
  return result;
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
