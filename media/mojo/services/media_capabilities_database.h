// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_CAPABILITIES_DATABASE_H_
#define MEDIA_MOJO_SERVICES_MEDIA_CAPABILITIES_DATABASE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"
#include "media/mojo/services/media_mojo_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This defines the interface to be used by various media capabilities services
// to store/retrieve decoding performance statistics.
class MEDIA_MOJO_EXPORT MediaCapabilitiesDatabase {
 public:
  // Representation of the information used to identify a type of media
  // playback.
  class MEDIA_MOJO_EXPORT Entry {
   public:
    Entry(VideoCodecProfile codec_profile,
          const gfx::Size& size,
          int frame_rate);

    VideoCodecProfile codec_profile() const { return codec_profile_; }
    const gfx::Size& size() const { return size_; }
    int frame_rate() const { return frame_rate_; }

   private:
    VideoCodecProfile codec_profile_;
    gfx::Size size_;
    int frame_rate_;
  };

  // Information saves to identify the capabilities related to a given |Entry|.
  struct MEDIA_MOJO_EXPORT Info {
    Info(uint32_t frames_decoded, uint32_t frames_dropped);
    uint32_t frames_decoded;
    uint32_t frames_dropped;
  };

  virtual ~MediaCapabilitiesDatabase() {}

  // Adds `info` data into the database records associated with `entry`. The
  // operation is asynchronous. The caller should be aware of potential race
  // conditions when calling this method for the same `entry` very close to
  // each others.
  virtual void AppendInfoToEntry(const Entry& entry, const Info& info) = 0;

  // Returns the `info` associated with `entry`. The `callback` will received
  // the `info` in addition to a boolean signaling if the call was successful.
  // `info` can be nullptr if there was no data associated with `entry`.
  using GetInfoCallback = base::OnceCallback<void(bool, std::unique_ptr<Info>)>;
  virtual void GetInfo(const Entry& entry, GetInfoCallback callback) = 0;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_CAPABILITIES_DATABASE_H_