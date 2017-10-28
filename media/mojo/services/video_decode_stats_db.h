// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_H_
#define MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"
#include "media/mojo/services/media_mojo_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This defines the interface to be used by various media capabilities services
// to store/retrieve video decoding performance statistics.
class MEDIA_MOJO_EXPORT VideoDecodeStatsDB {
 public:
  // Simple description of video decode complexity, serving as a key to look up
  // associated DecodeStatsEntries in the database.
  struct MEDIA_MOJO_EXPORT VideoDescKey {
    VideoDescKey(VideoCodecProfile codec_profile,
                 const gfx::Size& size,
                 int frame_rate);
    VideoCodecProfile codec_profile;
    gfx::Size size;
    int frame_rate;
  };

  // DecodeStatsEntry saved to identify the capabilities related to a given
  // |VideoDescKey|.
  struct MEDIA_MOJO_EXPORT DecodeStatsEntry {
    DecodeStatsEntry(uint64_t frames_decoded, uint64_t frames_dropped);
    uint64_t frames_decoded;
    uint64_t frames_dropped;
  };

  virtual ~VideoDecodeStatsDB() = default;

  // Run asynchronous initialization of database. Initialization must complete
  // before calling other APIs. |init_cb| must not be a null callback.
  virtual void Initialize(base::OnceCallback<void(bool)> init_cb) = 0;

  // Appends `stats` to existing entry associated with `key`. Will create a new
  // entry if none exists. The operation is asynchronous. The caller should be
  // aware of potential race conditions when calling this method for the same
  // `key` very close to other calls.
  virtual void AppendDecodeStats(const VideoDescKey& key,
                                 const DecodeStatsEntry& entry) = 0;

  // Returns the stats  associated with `key`. The `callback` will receive
  // the stats in addition to a boolean signaling if the call was successful.
  // DecodeStatsEntry can be nullptr if there was no data associated with `key`.
  using GetDecodeStatsCB =
      base::OnceCallback<void(bool, std::unique_ptr<DecodeStatsEntry>)>;
  virtual void GetDecodeStats(const VideoDescKey& key,
                              GetDecodeStatsCB callback) = 0;
};

// Factory interface to create a DB instance.
class MEDIA_MOJO_EXPORT VideoDecodeStatsDBFactory {
 public:
  virtual ~VideoDecodeStatsDBFactory() {}
  virtual std::unique_ptr<VideoDecodeStatsDB> CreateDB() = 0;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_H_
