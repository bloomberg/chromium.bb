// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_demuxer_factory.h"

namespace media {

FFmpegDemuxerFactory::FFmpegDemuxerFactory(
    const scoped_refptr<DataSource>& data_source,
    MessageLoop* loop)
    : data_source_(data_source),
      loop_(loop) {
}

FFmpegDemuxerFactory::~FFmpegDemuxerFactory() {}

static void DemuxerInitDone(const DemuxerFactory::BuildCallback& cb,
                            const scoped_refptr<FFmpegDemuxer>& demuxer,
                            PipelineStatus status) {
  if (status != PIPELINE_OK) {
    cb.Run(status, NULL);
    return;
  }
  cb.Run(PIPELINE_OK, demuxer);
}

void FFmpegDemuxerFactory::Build(const std::string& url,
                                 const BuildCallback& cb) {
  GURL gurl = GURL(url);
  bool local_source = !gurl.SchemeIs("http") && !gurl.SchemeIs("https");
  scoped_refptr<FFmpegDemuxer> demuxer = new FFmpegDemuxer(loop_, local_source);

  demuxer->Initialize(
      data_source_,
      base::Bind(&DemuxerInitDone, cb, demuxer));
}

}  // namespace media
