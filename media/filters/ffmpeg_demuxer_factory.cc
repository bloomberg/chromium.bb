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

// This class is a one-off whose raison d'etre is the lack of
// currying functionality in base/callback_old.h's machinery.  Once media/
// DemuxerFactory::BuildCallback is migrated to the new base/callback.h
// machinery this should be removed and replaced with currying calls to
// base::Bind().
class DemuxerCallbackAsPipelineStatusCallback : public PipelineStatusCallback {
 public:
  DemuxerCallbackAsPipelineStatusCallback(
      DemuxerFactory::BuildCallback* cb,
      Demuxer* demuxer)
      : cb_(cb), demuxer_(demuxer) {
    DCHECK(cb_.get() && demuxer_);
  }

  virtual ~DemuxerCallbackAsPipelineStatusCallback() {}

  virtual void RunWithParams(const Tuple1<PipelineStatus>& params) {
    cb_->Run(params.a, demuxer_);
  }

 private:
  scoped_ptr<DemuxerFactory::BuildCallback> cb_;
  scoped_refptr<Demuxer> demuxer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DemuxerCallbackAsPipelineStatusCallback);
};

static void DataSourceFactoryDone(DemuxerFactory::BuildCallback* callback,
                                  MessageLoop* loop,
                                  PipelineStatus status,
                                  DataSource* data_source) {
  scoped_ptr<DemuxerFactory::BuildCallback> cb(callback);
  if (status != PIPELINE_OK) {
    cb->Run(status, static_cast<Demuxer*>(NULL));
    return;
  }
  DCHECK(data_source);
  scoped_refptr<FFmpegDemuxer> demuxer = new FFmpegDemuxer(loop);
  demuxer->Initialize(
      data_source,
      new DemuxerCallbackAsPipelineStatusCallback(cb.release(), demuxer));
}

void FFmpegDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  data_source_factory_->Build(url,
                              base::Bind(&DataSourceFactoryDone, cb, loop_));
}

DemuxerFactory* FFmpegDemuxerFactory::Clone() const {
  return new FFmpegDemuxerFactory(data_source_factory_->Clone(), loop_);
}

}  // namespace media
