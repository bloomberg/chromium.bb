// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the DemuxerFactory interface using FFmpegDemuxer.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "media/base/filter_factories.h"

class MessageLoop;

namespace media {

class FFmpegDemuxerFactory : public DemuxerFactory {
 public:
  // Takes ownership of |data_source_factory|, but not of |loop|.
  FFmpegDemuxerFactory(DataSourceFactory* data_source_factory,
                       MessageLoop* loop);
  virtual ~FFmpegDemuxerFactory();

  virtual void Build(const std::string& url, BuildCallback* cb);
  virtual DemuxerFactory* Clone() const;

 private:
  scoped_ptr<DataSourceFactory> data_source_factory_;
  MessageLoop* loop_;  // Unowned.

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_FACTORY_H_
