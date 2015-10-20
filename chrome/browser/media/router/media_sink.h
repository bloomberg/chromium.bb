// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_

#include <string>

namespace media_router {

// Represents a sink to which media can be routed.
class MediaSink {
 public:
  using Id = std::string;

  enum IconType {
    CAST,
    CAST_AUDIO,
    CAST_AUDIO_GROUP,
    GENERIC,
    HANGOUT
  };

  MediaSink(const MediaSink::Id& sink_id,
            const std::string& name,
            const IconType icon_type);

  ~MediaSink();

  const MediaSink::Id& id() const { return sink_id_; }
  const std::string& name() const { return name_; }
  MediaSink::IconType icon_type() const { return icon_type_; }

  bool Equals(const MediaSink& other) const;
  bool Empty() const;

 private:
  // Unique identifier for the MediaSink.
  MediaSink::Id sink_id_;
  // Descriptive name of the MediaSink.
  // Optional, can use an empty string if no sink name is available.
  std::string name_;
  // The type of icon that corresponds with the MediaSink.
  MediaSink::IconType icon_type_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
