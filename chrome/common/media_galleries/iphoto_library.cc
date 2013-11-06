// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/iphoto_library.h"

namespace iphoto {
namespace parser {

Photo::Photo()
    : id(0) {
}

Photo::Photo(uint64 id,
             const base::FilePath& location,
             const base::FilePath& original_location)
    : id(id),
      location(location),
      original_location(original_location) {
}

bool Photo::operator<(const Photo& other) const {
  return id < other.id;
}

Library::Library() {}

Library::Library(const Albums& albums,
                 const std::set<Photo>& all_photos)
    : albums(albums),
      all_photos(all_photos) {}

Library::~Library() {}


}  // namespace parser
}  // namespace iphoto
