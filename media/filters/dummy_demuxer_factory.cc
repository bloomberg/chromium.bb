// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dummy_demuxer_factory.h"

#include "base/memory/scoped_ptr.h"
#include "media/filters/dummy_demuxer.h"

namespace media {

DummyDemuxerFactory::DummyDemuxerFactory(bool has_video, bool has_audio)
    : has_video_(has_video),
      has_audio_(has_audio) {
}

DummyDemuxerFactory::~DummyDemuxerFactory() {}

void DummyDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  scoped_refptr<DummyDemuxer> demuxer =
      new DummyDemuxer(has_video_, has_audio_);
  scoped_ptr<DemuxerFactory::BuildCallback> callback(cb);
  callback->Run(PIPELINE_OK, demuxer.get());
}

DemuxerFactory* DummyDemuxerFactory::Clone() const {
  return new DummyDemuxerFactory(has_video_, has_audio_);
}

}  // namespace media
