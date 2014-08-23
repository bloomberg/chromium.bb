// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"

#include "media/base/demuxer.h"
#include "media/base/renderer.h"
#include "media/base/text_renderer.h"

namespace media {

FilterCollection::FilterCollection() : demuxer_(NULL) {}

FilterCollection::~FilterCollection() {}

void FilterCollection::SetDemuxer(Demuxer* demuxer) {
  demuxer_ = demuxer;
}

Demuxer* FilterCollection::GetDemuxer() {
  return demuxer_;
}

void FilterCollection::SetRenderer(scoped_ptr<Renderer> renderer) {
  renderer_ = renderer.Pass();
}

scoped_ptr<Renderer> FilterCollection::GetRenderer() {
  return renderer_.Pass();
}

void FilterCollection::SetTextRenderer(scoped_ptr<TextRenderer> text_renderer) {
  text_renderer_ = text_renderer.Pass();
}

scoped_ptr<TextRenderer> FilterCollection::GetTextRenderer() {
  return text_renderer_.Pass();
}

}  // namespace media
