// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/factory.h"

namespace media {

FilterFactory::FilterFactory() {}

FilterFactory::~FilterFactory() {}

FilterFactoryCollection::FilterFactoryCollection() {}

void FilterFactoryCollection::AddFactory(FilterFactory* factory) {
  factories_.push_back(factory);
}

MediaFilter* FilterFactoryCollection::Create(FilterType filter_type,
                                             const MediaFormat& media_format) {
  MediaFilter* filter = NULL;
  for (FactoryVector::iterator factory = factories_.begin();
       !filter && factory != factories_.end();
       ++factory) {
    filter = (*factory)->Create(filter_type, media_format);
  }
  return filter;
}

FilterFactoryCollection::~FilterFactoryCollection() {}


}  // namespace media
