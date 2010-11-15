// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_filter_collection.h"

namespace media {

MediaFilterCollection::MediaFilterCollection() {
}

void MediaFilterCollection::AddFilter(MediaFilter* filter) {
  filters_.push_back(filter);
}

bool MediaFilterCollection::IsEmpty() const {
  return filters_.empty();
}

void MediaFilterCollection::Clear() {
  filters_.clear();
}

void MediaFilterCollection::SelectFilter(
    FilterType filter_type,
    scoped_refptr<MediaFilter>* filter_out)  {

  std::list<scoped_refptr<MediaFilter> >::iterator it = filters_.begin();
  while (it != filters_.end()) {
    if ((*it)->filter_type() == filter_type)
      break;
    ++it;
  }

  if (it != filters_.end()) {
    *filter_out = it->get();
    filters_.erase(it);
  }
}

}  // namespace media
