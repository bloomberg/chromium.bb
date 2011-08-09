// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_demuxer_factory.h"

namespace media {

static void DemuxerInitDone(DemuxerFactory::BuildCallback* cb,
                            const scoped_refptr<FFmpegDemuxer>& demuxer,
                            PipelineStatus status) {
  scoped_ptr<DemuxerFactory::BuildCallback> callback(cb);
  if (status != PIPELINE_OK) {
    callback->Run(status, static_cast<Demuxer*>(NULL));
    return;
  }

  callback->Run(PIPELINE_OK, demuxer.get());
}


FFmpegDemuxerFactory::FFmpegDemuxerFactory(
    DataSourceFactory* data_source_factory,
    MessageLoop* loop)
    : data_source_factory_(data_source_factory), loop_(loop) {}

FFmpegDemuxerFactory::~FFmpegDemuxerFactory() {}

// This class is a one-off whose raison d'etre is the lack of
// currying functionality in base/callback_old.h's machinery.  Once
// {DataSource,Demuxer}Factory::BuildCallback are migrated to the new
// base/callback.h machinery these should be removed and replaced
// with currying calls to base::Bind().
class DemuxerCallbackAsDataSourceCallback
    : public DataSourceFactory::BuildCallback {
 public:
  DemuxerCallbackAsDataSourceCallback(DemuxerFactory::BuildCallback* cb,
                                      MessageLoop* loop)
      : cb_(cb), loop_(loop) {
    DCHECK(cb_.get() && loop_);
  }

  virtual ~DemuxerCallbackAsDataSourceCallback() {}

  virtual void RunWithParams(
      const Tuple2<PipelineStatus, DataSource*>& params) {
    PipelineStatus status = params.a;
    DataSource* data_source = params.b;
    if (status != PIPELINE_OK) {
      cb_->Run(status, static_cast<Demuxer*>(NULL));
      return;
    }
    DCHECK(data_source);
    scoped_refptr<FFmpegDemuxer> demuxer = new FFmpegDemuxer(loop_);
    demuxer->Initialize(
        data_source,
        base::Bind(&DemuxerInitDone, cb_.release(), demuxer));
  }

 private:
  scoped_ptr<DemuxerFactory::BuildCallback> cb_;
  MessageLoop* loop_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DemuxerCallbackAsDataSourceCallback);
};

void FFmpegDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  data_source_factory_->Build(
      url, new DemuxerCallbackAsDataSourceCallback(cb, loop_));
}

DemuxerFactory* FFmpegDemuxerFactory::Clone() const {
  return new FFmpegDemuxerFactory(data_source_factory_->Clone(), loop_);
}

}  // namespace media
