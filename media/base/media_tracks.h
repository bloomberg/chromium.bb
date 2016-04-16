// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TRACKS_H_
#define MEDIA_BASE_MEDIA_TRACKS_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_track.h"

namespace media {

class AudioDecoderConfig;
class DemuxerStream;
class VideoDecoderConfig;

class MEDIA_EXPORT MediaTracks {
 public:
  typedef std::vector<scoped_ptr<MediaTrack>> MediaTracksCollection;
  typedef std::map<unsigned, const DemuxerStream*> TrackIdToDemuxStreamMap;

  MediaTracks();
  ~MediaTracks();

  // Callers need to ensure that track id is unique.
  const MediaTrack* AddAudioTrack(const AudioDecoderConfig& config,
                                  const std::string& id,
                                  const std::string& kind,
                                  const std::string& label,
                                  const std::string& language);
  // Callers need to ensure that track id is unique.
  const MediaTrack* AddVideoTrack(const VideoDecoderConfig& config,
                                  const std::string& id,
                                  const std::string& kind,
                                  const std::string& label,
                                  const std::string& language);

  const MediaTracksCollection& tracks() const { return tracks_; }

  // TODO(servolk,wolenetz): Consider refactoring media track creation in MSE to
  // simplify track id to DemuxerStream mapping. crbug.com/604088

  // Notifies MediaTracks that a given media |track| object is backed by the
  // given DemuxerStream |stream| object.
  void SetDemuxerStreamForMediaTrack(const MediaTrack* track,
                                     const DemuxerStream* stream);
  // Notifies MediaTracks that external (blink) track ids have been assigned to
  // the media |tracks_|. The size and ordering of |track_ids| must match the
  // size and ordering of tracks in the |tracks_| collection, and
  // |track_to_demux_stream_map_| must have an entry for each track in |tracks_|
  // (set by SetDemuxerStreamForMediaTrack()).
  TrackIdToDemuxStreamMap OnTrackIdsAssigned(
      const std::vector<unsigned>& track_ids) const;

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

  typedef std::map<const MediaTrack*, const DemuxerStream*>
      TrackToDemuxStreamMap;
  TrackToDemuxStreamMap track_to_demux_stream_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaTracks);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TRACKS_H_
