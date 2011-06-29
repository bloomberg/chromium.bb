// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer_factory.h"

#include "base/message_loop.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

MediaDataSink::MediaDataSink(const scoped_refptr<ChunkDemuxer>& demuxer)
    : demuxer_(demuxer) {
}

MediaDataSink::~MediaDataSink() {}

void MediaDataSink::Flush() {
  demuxer_->FlushData();
}

bool MediaDataSink::AddData(const uint8* data, unsigned length) {
  return demuxer_->AddData(data, length);
}

void MediaDataSink::Shutdown() {
  demuxer_->Shutdown();
}

const char ChunkDemuxerFactory::kURLPrefix[] = "x-media-chunks:";

class ChunkDemuxerFactory::BuildState
    : public base::RefCountedThreadSafe<BuildState> {
 public:
  static const int64 kMaxInfoSize = 32678;

  BuildState(const std::string& url, BuildCallback* cb,
             const scoped_refptr<ChunkDemuxer>& demuxer)
      : url_(url),
        cb_(cb),
        demuxer_(demuxer),
        read_buffer_(NULL),
        message_loop_(MessageLoop::current()) {
    AddRef();
  }

  virtual ~BuildState() {}

  void OnBuildDone(PipelineStatus status, DataSource* data_source) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &BuildState::DoBuildDone,
                          status, scoped_refptr<DataSource>(data_source)));
  }

  void DoBuildDone(PipelineStatus status, DataSource* data_source) {
    if (status != PIPELINE_OK) {
      cb_->Run(status, static_cast<Demuxer*>(NULL));
      Release();
      return;
    }

    data_source_ = data_source;

    int64 size = 0;

    if (!data_source_->GetSize(&size) || size >= kMaxInfoSize) {
      RunCallbackAndStop(DEMUXER_ERROR_COULD_NOT_OPEN);
      return;
    }

    DCHECK(!read_buffer_.get());
    read_buffer_.reset(new uint8[size]);
    data_source_->Read(0, size, read_buffer_.get(),
                       NewCallback(this, &BuildState::OnReadDone));
  }

  void OnReadDone(size_t size) {
    message_loop_->PostTask(FROM_HERE,
                            NewRunnableMethod(this,
                                              &BuildState::DoReadDone,
                                              size));
  }

  void DoReadDone(size_t size) {
    if (size == DataSource::kReadError) {
      RunCallbackAndStop(PIPELINE_ERROR_READ);
      return;
    }

    if (!demuxer_->Init(read_buffer_.get(), size)) {
      RunCallbackAndStop(DEMUXER_ERROR_COULD_NOT_OPEN);
      return;
    }

    RunCallbackAndStop(PIPELINE_OK);
  }

  void RunCallbackAndStop(PipelineStatus status) {
    scoped_refptr<Demuxer> demuxer;

    if (status == PIPELINE_OK)
      demuxer = demuxer_.get();

    cb_->Run(status, demuxer.get());
    data_source_->Stop(NewCallback(this, &BuildState::OnStopDone));
  }

  void OnStopDone() {
    message_loop_->PostTask(FROM_HERE,
                            NewRunnableMethod(this,
                                              &BuildState::DoStopDone));
  }

  void DoStopDone() { Release(); }

 private:
  std::string url_;
  scoped_ptr<BuildCallback> cb_;
  scoped_refptr<ChunkDemuxer> demuxer_;

  scoped_refptr<DataSource> data_source_;
  scoped_array<uint8> read_buffer_;
  MessageLoop* message_loop_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BuildState);
};

ChunkDemuxerFactory::ChunkDemuxerFactory(
    DataSourceFactory* data_source_factory)
    : data_source_factory_(data_source_factory) {
}

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

  std::string info_url = url.substr(strlen(kURLPrefix));

  data_source_factory_->Build(
      info_url,
      NewCallback(new BuildState(info_url, cb, demuxer_),
                  &ChunkDemuxerFactory::BuildState::OnBuildDone));
  demuxer_ = NULL;
}

DemuxerFactory* ChunkDemuxerFactory::Clone() const {
  return new ChunkDemuxerFactory(data_source_factory_->Clone());
}

}  // namespace media
