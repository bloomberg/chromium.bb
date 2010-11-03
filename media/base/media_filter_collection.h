// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_FILTER_COLLECTION_H_
#define MEDIA_BASE_MEDIA_FILTER_COLLECTION_H_

#include <list>

#include "base/ref_counted.h"
#include "media/base/filters.h"

namespace media {

// This is a collection of MediaFilter objects used to form a media playback
// pipeline. See src/media/base/pipeline.h for more information.
class MediaFilterCollection {
 public:
  MediaFilterCollection();

  // Adds a filter to the collection.
  void AddFilter(MediaFilter* filter);

  // Is the collection empty?
  bool IsEmpty() const;

  // Remove remaining filters.
  void Clear();

  // Selects a filter of the specified type from the collection.
  // If the required filter cannot be found, NULL is returned.
  // If a filter is returned it is removed from the collection.
  template <class Filter>
  void SelectFilter(scoped_refptr<Filter>* filter_out) {
    scoped_refptr<MediaFilter> filter;
    SelectFilter(Filter::static_filter_type(), &filter);
    *filter_out = reinterpret_cast<Filter*>(filter.get());
  }

 private:
  // List of filters managed by this collection.
  std::list<scoped_refptr<MediaFilter> > filters_;

  // Helper function that searches the filters list for a specific
  // filter type.
  void SelectFilter(FilterType filter_type,
                    scoped_refptr<MediaFilter>* filter_out);

  DISALLOW_COPY_AND_ASSIGN(MediaFilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_FILTER_COLLECTION_H_
