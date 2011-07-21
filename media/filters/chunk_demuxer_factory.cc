// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer_factory.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

static void DoInitDone(DemuxerFactory::BuildCallback* cb,
                       const scoped_refptr<Demuxer>& demuxer,
                       PipelineStatus status) {
  scoped_ptr<DemuxerFactory::BuildCallback> callback(cb);
  if (status != PIPELINE_OK) {
    callback->Run(status, static_cast<Demuxer*>(NULL));
    return;
  }

  callback->Run(status, demuxer);
}

static void InitDone(MessageLoop* message_loop,
                     DemuxerFactory::BuildCallback* cb,
                     const scoped_refptr<Demuxer>& demuxer,
                     PipelineStatus status) {
  message_loop->PostTask(FROM_HERE,
                         NewRunnableFunction(&DoInitDone, cb, demuxer, status));
}

ChunkDemuxerFactory::ChunkDemuxerFactory(const std::string& url,
                                         DemuxerFactory* delegate_factory,
                                         ChunkDemuxerClient* client)
    : url_(url),
      delegate_factory_(delegate_factory),
      client_(client) {
  DCHECK(delegate_factory_.get());
}

ChunkDemuxerFactory::~ChunkDemuxerFactory() {}

void ChunkDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  // Check to see if this is the URL we are looking for. If not delegate
  // building to the delegate factory.
  if (url != url_) {
    delegate_factory_->Build(url, cb);
    return;
  }

  scoped_refptr<ChunkDemuxer> demuxer(new ChunkDemuxer(client_));
  // Call Init() on demuxer. Note that ownership is being passed to the
  // callback here.
  demuxer->Init(base::Bind(&InitDone, MessageLoop::current(), cb,
                           scoped_refptr<Demuxer>(demuxer.get())));
}

DemuxerFactory* ChunkDemuxerFactory::Clone() const {
  return new ChunkDemuxerFactory(url_, delegate_factory_->Clone(), client_);
}

}  // namespace media
