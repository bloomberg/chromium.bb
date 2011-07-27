// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_demuxer_factory.h"

namespace media {

FFmpegDemuxerFactory::FFmpegDemuxerFactory(
    DataSourceFactory* data_source_factory,
    MessageLoop* loop)
    : data_source_factory_(data_source_factory), loop_(loop) {}

FFmpegDemuxerFactory::~FFmpegDemuxerFactory() {}

static void DemuxerInitDone(const DemuxerFactory::BuildCB& cb,
                            const scoped_refptr<Demuxer>& demuxer,
                            PipelineStatus status) {
  cb.Run(status, demuxer.get());
}

static void DataSourceFactoryDone(const DemuxerFactory::BuildCB& cb,
                                  MessageLoop* loop,
                                  PipelineStatus status,
                                  DataSource* data_source) {
  if (status != PIPELINE_OK) {
    cb.Run(status, NULL);
    return;
  }
  DCHECK(data_source);
  scoped_refptr<FFmpegDemuxer> demuxer = new FFmpegDemuxer(loop);
  demuxer->Initialize(
      data_source,
      base::Bind(&DemuxerInitDone, cb, demuxer));
}

void FFmpegDemuxerFactory::Build(const std::string& url, const BuildCB& cb) {
  data_source_factory_->Build(url,
                              base::Bind(&DataSourceFactoryDone, cb, loop_));
}

DemuxerFactory* FFmpegDemuxerFactory::Clone() const {
  return new FFmpegDemuxerFactory(data_source_factory_->Clone(), loop_);
}

}  // namespace media
