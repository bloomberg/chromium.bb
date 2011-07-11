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

MediaDataSink::MediaDataSink(const scoped_refptr<ChunkDemuxer>& demuxer)
    : demuxer_(demuxer) {
}

MediaDataSink::~MediaDataSink() {}

void MediaDataSink::Flush() {
  demuxer_->FlushData();
}

bool MediaDataSink::AppendData(const uint8* data, unsigned length) {
  return demuxer_->AppendData(data, length);
}

void MediaDataSink::Shutdown() {
  demuxer_->Shutdown();
}

const char ChunkDemuxerFactory::kURLPrefix[] = "x-media-chunks:";

ChunkDemuxerFactory::ChunkDemuxerFactory() {}

ChunkDemuxerFactory::~ChunkDemuxerFactory() {}

bool ChunkDemuxerFactory::IsUrlSupported(const std::string& url) const {
  return (url.find(kURLPrefix) == 0);
}

MediaDataSink* ChunkDemuxerFactory::CreateMediaDataSink() {
  demuxer_ = new ChunkDemuxer();
  return new MediaDataSink(demuxer_);
}

void ChunkDemuxerFactory::Build(const std::string& url, BuildCallback* cb) {
  if (!IsUrlSupported(url) || !demuxer_.get()) {
    cb->Run(DEMUXER_ERROR_COULD_NOT_OPEN, static_cast<Demuxer*>(NULL));
    delete cb;
    return;
  }

  // Call Init() on demuxer. Note that ownership is being passed to the
  // callback here.
  demuxer_->Init(base::Bind(&InitDone, MessageLoop::current(), cb,
                            scoped_refptr<Demuxer>(demuxer_.get())));
  demuxer_ = NULL;
}

DemuxerFactory* ChunkDemuxerFactory::Clone() const {
  return new ChunkDemuxerFactory();
}

}  // namespace media
