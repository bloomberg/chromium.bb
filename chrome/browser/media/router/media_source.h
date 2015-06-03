// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_

#include <ostream>
#include <string>

#include "base/hash.h"

namespace media_router {


class MediaSource {
 public:
  using Id = std::string;

  explicit MediaSource(const MediaSource::Id& id);
  MediaSource();
  ~MediaSource();

  // Gets the ID of the media source.
  MediaSource::Id id() const;

  // Returns true if two MediaSource objects use the same media ID.
  bool Equals(const MediaSource& other) const;

  // Returns true if a MediaSource is empty or uninitialized.
  bool Empty() const;

  // Used for logging.
  std::string ToString() const;

  // Hash operator for hash containers.
  struct Hash {
    size_t operator()(const MediaSource& source) const {
      return base::Hash(source.id());
    }
  };

 private:
  MediaSource::Id id_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_
