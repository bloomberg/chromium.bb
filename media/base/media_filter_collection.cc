// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_filter_collection.h"

namespace media {

MediaFilterCollection::MediaFilterCollection() {
}

void MediaFilterCollection::AddDataSource(DataSource* filter) {
  AddFilter(DATA_SOURCE, filter);
}

void MediaFilterCollection::AddDemuxer(Demuxer* filter) {
  AddFilter(DEMUXER, filter);
}

void MediaFilterCollection::AddVideoDecoder(VideoDecoder* filter) {
  AddFilter(VIDEO_DECODER, filter);
}

void MediaFilterCollection::AddAudioDecoder(AudioDecoder* filter) {
  AddFilter(AUDIO_DECODER, filter);
}

void MediaFilterCollection::AddVideoRenderer(VideoRenderer* filter) {
  AddFilter(VIDEO_RENDERER, filter);
}

void MediaFilterCollection::AddAudioRenderer(AudioRenderer* filter) {
  AddFilter(AUDIO_RENDERER, filter);
}

bool MediaFilterCollection::IsEmpty() const {
  return filters_.empty();
}

void MediaFilterCollection::Clear() {
  filters_.clear();
}

void MediaFilterCollection::SelectDataSource(
    scoped_refptr<DataSource>* filter_out) {
  SelectFilter<DATA_SOURCE>(filter_out);
}

void MediaFilterCollection::SelectDemuxer(scoped_refptr<Demuxer>* filter_out) {
  SelectFilter<DEMUXER>(filter_out);
}

void MediaFilterCollection::SelectVideoDecoder(
    scoped_refptr<VideoDecoder>* filter_out) {
  SelectFilter<VIDEO_DECODER>(filter_out);
}

void MediaFilterCollection::SelectAudioDecoder(
    scoped_refptr<AudioDecoder>* filter_out) {
  SelectFilter<AUDIO_DECODER>(filter_out);
}

void MediaFilterCollection::SelectVideoRenderer(
    scoped_refptr<VideoRenderer>* filter_out) {
  SelectFilter<VIDEO_RENDERER>(filter_out);
}

void MediaFilterCollection::SelectAudioRenderer(
    scoped_refptr<AudioRenderer>* filter_out) {
  SelectFilter<AUDIO_RENDERER>(filter_out);
}

void MediaFilterCollection::AddFilter(FilterType filter_type,
                                      MediaFilter* filter) {
  filters_.push_back(FilterListElement(filter_type, filter));
}

template<MediaFilterCollection::FilterType filter_type, class Filter>
void MediaFilterCollection::SelectFilter(scoped_refptr<Filter>* filter_out) {
  scoped_refptr<MediaFilter> filter;
  SelectFilter(filter_type, &filter);
  *filter_out = reinterpret_cast<Filter*>(filter.get());
}

void MediaFilterCollection::SelectFilter(
    FilterType filter_type,
    scoped_refptr<MediaFilter>* filter_out)  {

  FilterList::iterator it = filters_.begin();
  while (it != filters_.end()) {
    if (it->first == filter_type)
      break;
    ++it;
  }

  if (it != filters_.end()) {
    *filter_out = it->second.get();
    filters_.erase(it);
  }
}

}  // namespace media
