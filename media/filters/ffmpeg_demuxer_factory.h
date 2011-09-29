// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the DemuxerFactory interface using FFmpegDemuxer.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/filter_factories.h"

class MessageLoop;

namespace media {

class MEDIA_EXPORT FFmpegDemuxerFactory : public DemuxerFactory {
 public:
  // Takes ownership of |data_source_factory|, but not of |loop|.
  FFmpegDemuxerFactory(DataSourceFactory* data_source_factory,
                       MessageLoop* loop);
  virtual ~FFmpegDemuxerFactory();

  // DemuxerFactory methods.
  virtual void Build(const std::string& url, const BuildCallback& cb) OVERRIDE;
  virtual DemuxerFactory* Clone() const OVERRIDE;

 private:
  scoped_ptr<DataSourceFactory> data_source_factory_;
  MessageLoop* loop_;  // Unowned.

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_
