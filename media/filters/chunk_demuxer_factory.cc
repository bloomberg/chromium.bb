// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer_factory.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

static void InitDone(MessageLoop* message_loop,
                     const DemuxerFactory::BuildCallback& cb,
                     scoped_refptr<Demuxer> demuxer,
                     PipelineStatus status) {
  if (status != PIPELINE_OK)
    demuxer = NULL;
  message_loop->PostTask(FROM_HERE, base::Bind(cb, status, demuxer));
}

ChunkDemuxerFactory::ChunkDemuxerFactory(ChunkDemuxerClient* client)
    : client_(client) {
  DCHECK(client_);
}

ChunkDemuxerFactory::~ChunkDemuxerFactory() {}

void ChunkDemuxerFactory::Build(const std::string& url,
                                const BuildCallback& cb) {
  scoped_refptr<ChunkDemuxer> demuxer(new ChunkDemuxer(client_));

  // Call Init() on demuxer. Note that ownership is being passed to the
  // callback here.
  demuxer->Init(base::Bind(&InitDone, MessageLoop::current(), cb, demuxer));
}

}  // namespace media
