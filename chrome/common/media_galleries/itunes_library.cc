// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/itunes_library.h"

namespace itunes {
namespace parser {

Track::Track()
    : id(0) {
}

Track::Track(uint64 id, const base::FilePath& location)
    : id(id),
      location(location) {
}

bool Track::operator<(const Track& other) const {
  return id < other.id;
}

}  // namespace parser
}  // namespace itunes
