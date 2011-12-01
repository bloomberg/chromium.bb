// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the DemuxerFactory interface using DummyDemuxer.

#ifndef MEDIA_FILTERS_DUMMY_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_DUMMY_DEMUXER_FACTORY_H_

#include "base/compiler_specific.h"
#include "media/base/filter_factories.h"

namespace media {

class MEDIA_EXPORT DummyDemuxerFactory : public DemuxerFactory {
 public:
  DummyDemuxerFactory(bool has_video, bool has_audio, bool local_source);
  virtual ~DummyDemuxerFactory();

  // DemuxerFactory methods.
  virtual void Build(const std::string& url, const BuildCallback& cb) OVERRIDE;
  virtual DemuxerFactory* Clone() const OVERRIDE;

 private:
  bool has_video_;
  bool has_audio_;
  bool local_source_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DummyDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DUMMY_DEMUXER_FACTORY_H_
