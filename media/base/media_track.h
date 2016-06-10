// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TRACK_H_
#define MEDIA_BASE_MEDIA_TRACK_H_

#include <string>

#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class MEDIA_EXPORT MediaTrack {
 public:
  enum Type { Text, Audio, Video };
  MediaTrack(Type type,
             StreamParser::TrackId bytestream_track_id,
             const std::string& kind,
             const std::string& label,
             const std::string& lang);
  ~MediaTrack();

  Type type() const { return type_; }

  StreamParser::TrackId bytestream_track_id() const {
    return bytestream_track_id_;
  }
  const std::string& kind() const { return kind_; }
  const std::string& label() const { return label_; }
  const std::string& language() const { return language_; }

 private:
  Type type_;
  StreamParser::TrackId bytestream_track_id_;
  std::string kind_;
  std::string label_;
  std::string language_;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TRACK_H_
