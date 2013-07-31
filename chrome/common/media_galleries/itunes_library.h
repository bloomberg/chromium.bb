// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These data structures can be used to describe the contents of an iTunes
// library.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_ITUNES_LIBRARY_H_
#define CHROME_COMMON_MEDIA_GALLERIES_ITUNES_LIBRARY_H_

#include <map>
#include <set>

#include "base/files/file_path.h"

namespace itunes {
namespace parser {

struct Track {
  Track();
  Track(uint64 id, const base::FilePath& location);
  bool operator<(const Track& other) const;

  uint64 id;
  base::FilePath location;
};

typedef std::set<Track> Album;
typedef std::map<std::string /*album name*/, Album> Albums;
typedef std::map<std::string /*artist name*/, Albums> Library;

}  // namespace parser
}  // namespace itunes

#endif  // CHROME_COMMON_MEDIA_GALLERIES_ITUNES_LIBRARY_H_

