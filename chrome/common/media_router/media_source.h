// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_

#include <iosfwd>
#include <string>

#include "base/hash.h"
#include "url/gurl.h"

// TODO(mfoltz): Right now this is a wrapper for std::string.  Factor methods
// from media_source_helper here so this object becomes useful; and don't just
// pass it around by Id.
namespace media_router {

class MediaSource {
 public:
  using Id = std::string;

  explicit MediaSource(const MediaSource::Id& id);
  explicit MediaSource(const GURL& presentation_url);
  MediaSource();
  ~MediaSource();

  // Gets the ID of the media source.
  MediaSource::Id id() const;

  // If MediaSource is created from a URL, return the URL; otherwise return an
  // empty GURL.
  GURL url() const;

  // Returns true if two MediaSource objects use the same media ID.
  bool operator==(const MediaSource& other) const;

  bool operator<(const MediaSource& other) const;

  // Hash operator for hash containers.
  struct Hash {
    uint32_t operator()(const MediaSource& source) const {
      return base::Hash(source.id());
    }
  };

 private:
  MediaSource::Id id_;
  GURL url_;
};

// Only for debug logging.  This operator is defined inline so it doesn't add
// any code in release builds.  (Omitting the definition entirely when NDEBUG is
// defined causes linker errors on Android.)
inline std::ostream& operator<<(std::ostream& stream,
                                const MediaSource& source) {
  return stream << "MediaSource[" << source.id() << "]";
}

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_SOURCE_H_
